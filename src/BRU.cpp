#include "../include/BRU.hpp"

namespace dark {

namespace {

inline int bruHead(const BRU &bru) {
	int best = -1;
	max_size_t bestTag = 0;
	for (int i = 0; i < BRU_CAP; ++i) {
		if (bru.slot_valid[i] == 0) {
			continue;
		}
		auto tag = to_unsigned(bru.slot_rob_tag[i]);
		if (best == -1 || tag < bestTag) {
			best = i;
			bestTag = tag;
		}
	}
	return best;
}

// ports BRU::BRUExecute (src/BRU/BRU.cpp:4-24)
inline bool takenOf(max_size_t op, max_size_t vj, max_size_t vk) {
	switch (static_cast<Operation>(op)) {
	case Operation::EQ:
		return vj == vk;
	case Operation::NE:
		return vj != vk;
	case Operation::LT:
		return static_cast<int32_t>(vj) < static_cast<int32_t>(vk);
	case Operation::GE:
		return static_cast<int32_t>(vj) >= static_cast<int32_t>(vk);
	case Operation::LTU:
		return vj < vk;
	case Operation::GEU:
		return vj >= vk;
	default:
		return false;
	}
}

} // namespace

bool BRU::isEmpty() const {
	for (int i = 0; i < BRU_CAP; ++i) {
		if (slot_valid[i]) {
			return false;
		}
	}
	return true;
}

bool BRU::isFull() const {
	for (int i = 0; i < BRU_CAP; ++i) {
		if (slot_valid[i] == 0) {
			return false;
		}
	}
	return true;
}

max_size_t BRU::peekTag() const {
	int i = bruHead(*this);
	return i >= 0 ? to_unsigned(slot_rob_tag[i]) : 0;
}

max_size_t BRU::peekPCFrom() const {
	int i = bruHead(*this);
	return i >= 0 ? to_unsigned(slot_pc_from[i]) : 0;
}

max_size_t BRU::peekPCResult() const {
	int i = bruHead(*this);
	return i >= 0 ? to_unsigned(slot_pc_result[i]) : 0;
}

bool BRU::acceptValid() const {
	if (cand_valid == 0 || isFull()) {
		return false;
	}
	if (squash.valid && to_unsigned(cand_tag) >= to_unsigned(squash.tag)) {
		return false;
	}
	return true;
}

max_size_t BRU::acceptIndex() const {
	return acceptValid() ? to_unsigned(cand_index) : 0;
}

void BRU::remove(max_size_t robTag) {
	for (int i = 0; i < BRU_CAP; ++i) {
		if (slot_valid[i] && to_unsigned(slot_rob_tag[i]) == robTag) {
			slot_valid[i] <= 0;
			return;
		}
	}
}

void BRU::flush(max_size_t flushTag) {
	for (int i = 0; i < BRU_CAP; ++i) {
		if (slot_valid[i] && to_unsigned(slot_rob_tag[i]) > flushTag) {
			slot_valid[i] <= 0;
		}
	}
}

void BRU::work() {
	// 1. squash flush
	if (squash.valid) {
		flush(to_unsigned(squash.tag));
	}
	// 2. cdb consume: head result processed (ROB write / squash decide next cycle).
	//    Skip when the result is younger than an in-flight squash: the flush
	//    above already clears those slots (original removes at writeBack and
	//    flushes next cycle - separate cycles; here both happen in one work())
	if (cdb_valid && (squash.valid == 0
					  || to_unsigned(cdb_tag) < to_unsigned(squash.tag))) {
		remove(to_unsigned(cdb_tag));
	}
	// 3. accept candidate (BRUExecute + push, src/BRU/BRU.cpp:4-24)
	if (acceptValid()) {
		bool taken = takenOf(to_unsigned(cand_op), to_unsigned(cand_vj),
							 to_unsigned(cand_vk));
		max_size_t pcResult = taken
			? (to_unsigned(cand_pc) + static_cast<int32_t>(to_unsigned(cand_imm)))
			: (to_unsigned(cand_pc) + 4);
		for (int i = 0; i < BRU_CAP; ++i) {
			if (slot_valid[i] == 0) {
				slot_valid[i] <= 1;
				slot_pc_from[i] <= to_unsigned(cand_pc);
				slot_pc_result[i] <= pcResult;
				slot_rob_tag[i] <= to_unsigned(cand_tag);
				break;
			}
		}
	}
}

} // namespace dark
