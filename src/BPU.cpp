#include "../include/BPU.hpp"

namespace dark {

PredictInfo BPU::predict(int32_t pc) const { // ports src/BPU/BPU.cpp:5-19
	auto local_index = (pc >> 2) & (BHT_CAP - 1);
	auto global_index = ((pc >> 2) ^ static_cast<int>(to_unsigned(GHR))) & (BHT_CAP - 1);
	auto selector_index = ((pc >> 2) ^ static_cast<int>(to_unsigned(GHR))) & (SELECTOR_CAP - 1);
	auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
	bool hit = static_cast<bool>(btb_valid[BTB_index])
		&& to_unsigned(btb_actual[BTB_index]) == static_cast<max_size_t>(pc);
	bool use_global = to_unsigned(selector[selector_index]) >= 2;
	bool taken = hit && (use_global ? to_unsigned(globalPHT[global_index]) >= 2
									: to_unsigned(localPHT[local_index]) >= 2);
	int32_t predictPC = pc + 4;
	if (taken && hit) {
		predictPC = static_cast<int32_t>(to_unsigned(btb_target[BTB_index]));
	}
	return {taken, predictPC};
}

BranchPredictorCkpt BPU::snapshotCheckPoint() const { // ports BPU.cpp:78-80
	return {static_cast<int>(to_unsigned(ras_top)),
			static_cast<uint16_t>(to_unsigned(GHR))};
}

bool BPU::RAS_empty() const {
	return to_unsigned(ras_top) == 0;
}

bool BPU::RAS_full() const {
	return to_unsigned(ras_top) == RAS_CAP;
}

max_size_t BPU::rasTopValue() const {
	auto top = to_unsigned(ras_top);
	if (top == 0) {
		return 0;
	}
	return to_unsigned(ras[top - 1]);
}

namespace {

// pending single-write target of one update() application (ports BPU.cpp:20-58)
struct PendingUpdate {
	bool valid = false;
	max_size_t li = 0, gi = 0, si = 0, bi = 0;
	max_size_t lv = 0, gv = 0, sv = 0;
	max_size_t b_actual = 0, b_target = 0;
	bool b_taken = false;
};

} // namespace

void BPU::work() {
	// 1. one-shot init: PHT/selector start at 1 (weakly not-taken,
	//    ports BranchPredictor constructor memset(1))
	if (static_cast<bool>(inited) == false) {
		for (int i = 0; i < BHT_CAP; ++i) {
			globalPHT[i] <= 1;
			localPHT[i] <= 1;
		}
		for (int i = 0; i < SELECTOR_CAP; ++i) {
			selector[i] <= 1;
		}
		inited <= 1;
	}
	// 2. squash recover (ports CPU.cpp:1042-1045: rollback to the squash
	//    entry's checkpoint). The squash cycle never fetches (fetch gated).
	if (squash.valid && recover_ok) {
		auto ckpt = to_unsigned(recover_ckpt);
		ras_top <= ((ckpt >> 16) & 0xFFu);
		GHR <= (ckpt & 0xFFFFu);
	}
	// 3. fetch-time state mutations (ports CPU.cpp:39-50): synchronous with
	//    the fetch because fetch_raw/fetch_valid are combined wires over the
	//    same old-state IMEM computes the fetch from
	if (fetch_valid && squash.valid == 0) {
		auto raw = to_unsigned(fetch_raw);
		auto opcode = raw & 0x7Fu;
		auto rd = (raw >> 7) & 0x1Fu;
		auto rs1 = (raw >> 15) & 0x1Fu;
		auto imm_i = (raw >> 20) & 0xFFFu;
		auto p = to_unsigned(pc);
		if (opcode == 0b1101111u && (rd == 1 || rd == 5)) { // JAL link: push ra
			if (RAS_full() == false) {
				ras[to_unsigned(ras_top)] <= p + 4;
				ras_top <= to_unsigned(ras_top) + 1;
			}
		} else if (opcode == 0b1100111u && rd == 0 && (rs1 == 1 || rs1 == 5)
				   && imm_i == 0) { // JALR link: pop ra
			if (RAS_empty() == false) {
				ras_top <= to_unsigned(ras_top) - 1;
			}
		}
		if (opcode == 0b1100011u) { // B: shift GHR with the prediction
			GHR <= (((to_unsigned(GHR) << 1) | (predict(static_cast<int32_t>(p)).taken ? 1u : 0u))
				& HISTORY_MASK);
		} else if (opcode == 0b1101111u || opcode == 0b1100111u) { // JAL/JALR
			GHR <= (((to_unsigned(GHR) << 1) | 1u) & HISTORY_MASK);
		}
	}
	// 4. resolution updates (ports CPU.cpp:842-843/903-904). Two sources can
	//    resolve in the same cycle: merge into one pending write with the CDB
	//    (JALR) path winning on a collision, matching the original order.
	auto applyUpdate = [this](PendingUpdate &p, int32_t pc, bool taken,
							  int32_t target, uint16_t ghr) {
		auto li = static_cast<max_size_t>((pc >> 2) & (BHT_CAP - 1));
		auto gi = static_cast<max_size_t>(((pc >> 2) ^ ghr) & (BHT_CAP - 1));
		auto si = static_cast<max_size_t>(((pc >> 2) ^ ghr) & (SELECTOR_CAP - 1));
		auto bi = static_cast<max_size_t>((pc >> 2) & (BTB_CAP - 1));
		bool pred_local = to_unsigned(localPHT[li]) >= 2;
		bool pred_global = to_unsigned(globalPHT[gi]) >= 2;
		auto old_l = to_unsigned(localPHT[li]);
		auto old_g = to_unsigned(globalPHT[gi]);
		auto old_s = to_unsigned(selector[si]);
		p.valid = true;
		p.li = li;
		p.gi = gi;
		p.si = si;
		p.bi = bi;
		p.lv = taken ? (old_l < 3 ? old_l + 1 : old_l)
					: (old_l > 0 ? old_l - 1 : old_l);
		p.gv = taken ? (old_g < 3 ? old_g + 1 : old_g)
					: (old_g > 0 ? old_g - 1 : old_g);
		if (pred_global && !pred_local) {
			p.sv = old_s < 3 ? old_s + 1 : old_s;
		} else if (pred_local && !pred_global) {
			p.sv = old_s > 0 ? old_s - 1 : old_s;
		} else {
			p.sv = old_s;
		}
		p.b_actual = static_cast<max_size_t>(pc);
		p.b_target = static_cast<max_size_t>(target);
		p.b_taken = taken;
	};
	// two pending updates: the original applies update() twice (BRU first,
	// then CDB); on a per-field index collision the later CDB write wins
	PendingUpdate pendBr;
	if (br.valid && (squash.valid == 0 || to_unsigned(br.tag) < to_unsigned(squash.tag))) {
		applyUpdate(pendBr, static_cast<int32_t>(to_unsigned(br.pc)),
					static_cast<bool>(br.taken),
					static_cast<int32_t>(to_unsigned(br.target)),
					static_cast<uint16_t>(to_unsigned(br.ghr)));
	}
	PendingUpdate pendCdb;
	if (cdb.valid && cdb.is_control
		&& (squash.valid == 0 || to_unsigned(cdb.tag) < to_unsigned(squash.tag))) {
		applyUpdate(pendCdb, static_cast<int32_t>(to_unsigned(cdb.pc)), true,
					static_cast<int32_t>(to_unsigned(cdb.target)),
					static_cast<uint16_t>(to_unsigned(cdb.ghr)));
	}
	if (static_cast<bool>(inited)) { // skip on the init cycle
		bool cdbOverlaps = pendCdb.valid;
		if (pendBr.valid) {
			if (!(cdbOverlaps && pendCdb.li == pendBr.li)) {
				localPHT[pendBr.li] <= pendBr.lv;
			}
			if (!(cdbOverlaps && pendCdb.gi == pendBr.gi)) {
				globalPHT[pendBr.gi] <= pendBr.gv;
			}
			if (!(cdbOverlaps && pendCdb.si == pendBr.si)) {
				selector[pendBr.si] <= pendBr.sv;
			}
			if (pendBr.b_taken && !(cdbOverlaps && pendCdb.bi == pendBr.bi)) {
				btb_actual[pendBr.bi] <= pendBr.b_actual;
				btb_target[pendBr.bi] <= pendBr.b_target;
				btb_valid[pendBr.bi] <= 1;
			}
		}
		if (pendCdb.valid) {
			localPHT[pendCdb.li] <= pendCdb.lv;
			globalPHT[pendCdb.gi] <= pendCdb.gv;
			selector[pendCdb.si] <= pendCdb.sv;
			if (pendCdb.b_taken) {
				btb_actual[pendCdb.bi] <= pendCdb.b_actual;
				btb_target[pendCdb.bi] <= pendCdb.b_target;
				btb_valid[pendCdb.bi] <= 1;
			}
		}
	}
}

} // namespace dark
