#include "../include/STORE_RS.hpp"

namespace dark {

bool STORE_RS::isFull() const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_busy[i] == 0) {
			return false;
		}
	}
	return true;
}

bool STORE_RS::isEmpty() const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_busy[i]) {
			return false;
		}
	}
	return true;
}

bool STORE_RS::isOperandFull() const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_operand_busy[i] == 0) {
			return false;
		}
	}
	return true;
}

bool STORE_RS::isOperandEmpty() const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_operand_busy[i]) {
			return false;
		}
	}
	return true;
}

bool STORE_RS::isBusy(int i) const {
	return static_cast<bool>(slot_busy[i]);
}

int STORE_RS::getIndex(max_size_t tag) const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_busy[i] && to_unsigned(slot_rob_tag[i]) == tag) {
			return i;
		}
	}
	return -1;
}

max_size_t STORE_RS::getOp(int i) const {
	return to_unsigned(slot_op[i]);
}

max_size_t STORE_RS::getVj(int i) const {
	return to_unsigned(slot_vj[i]);
}

max_size_t STORE_RS::getQj(int i) const {
	return to_unsigned(slot_qj[i]);
}

max_size_t STORE_RS::getVk(int i) const {
	return to_unsigned(slot_vk[i]);
}

max_size_t STORE_RS::getRobTag(int i) const {
	return to_unsigned(slot_rob_tag[i]);
}

bool STORE_RS::isOperandBusy(int i) const {
	return static_cast<bool>(slot_operand_busy[i]);
}

int STORE_RS::getOperandIndex(max_size_t tag) const {
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (slot_operand_busy[i] && to_unsigned(slot_operand_tag[i]) == tag) {
			return i;
		}
	}
	return -1;
}

max_size_t STORE_RS::getOperandValue(int i) const {
	return to_unsigned(slot_operand_value[i]);
}

max_size_t STORE_RS::getOperandQ(int i) const {
	return to_unsigned(slot_operand_q[i]);
}

max_size_t STORE_RS::getOperandTag(int i) const {
	return to_unsigned(slot_operand_tag[i]);
}

namespace {

// smallest rob_tag among ready address slots; returns -1 if none
inline int storeCandidate(const STORE_RS &rs, bool withSquash, max_size_t squashTag) {
	int best = -1;
	max_size_t bestTag = 0;
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (rs.slot_busy[i] == 0) {
			continue;
		}
		auto tag = to_unsigned(rs.slot_rob_tag[i]);
		if (to_unsigned(rs.slot_qj[i]) != 0) {
			continue; // address only checks qj (CPU.cpp:601)
		}
		if (withSquash && tag >= squashTag) {
			continue;
		}
		if (best == -1 || tag < bestTag) {
			best = i;
			bestTag = tag;
		}
	}
	return best;
}

} // namespace

bool STORE_RS::execCandidateValid() const {
	return storeCandidate(*this, static_cast<bool>(squash.valid),
						  to_unsigned(squash.tag)) >= 0;
}

max_size_t STORE_RS::execCandidateIndex() const {
	int i = storeCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag));
	return i >= 0 ? static_cast<max_size_t>(i) : 0;
}

max_size_t STORE_RS::execCandidateOp() const {
	int i = storeCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_op[i]) : 0;
}

max_size_t STORE_RS::execCandidateVj() const {
	int i = storeCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_vj[i]) : 0;
}

max_size_t STORE_RS::execCandidateVk() const {
	int i = storeCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_vk[i]) : 0;
}

max_size_t STORE_RS::execCandidateTag() const {
	int i = storeCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_rob_tag[i]) : 0;
}

bool STORE_RS::operandReadyValid(int i) const {
	if (i < 0 || i >= STORERS_CAP || slot_operand_busy[i] == 0) {
		return false;
	}
	if (to_unsigned(slot_operand_q[i]) != 0) {
		return false;
	}
	if (squash.valid && to_unsigned(slot_operand_tag[i]) >= to_unsigned(squash.tag)) {
		return false;
	}
	return true;
}

max_size_t STORE_RS::operandReadyValue(int i) const {
	return operandReadyValid(i) ? to_unsigned(slot_operand_value[i]) : 0;
}

max_size_t STORE_RS::operandReadyTag(int i) const {
	return operandReadyValid(i) ? to_unsigned(slot_operand_tag[i]) : 0;
}

void STORE_RS::work() {
	// 1. squash: free both slot sets younger than squash tag (CPU.cpp:990-996, 1005-1010)
	if (squash.valid) {
		auto sqTag = to_unsigned(squash.tag);
		for (int i = 0; i < STORERS_CAP; ++i) {
			if (slot_busy[i] && to_unsigned(slot_rob_tag[i]) > sqTag) {
				slot_busy[i] <= 0;
				slot_qj[i] <= 0;
			}
			if (slot_operand_busy[i] && to_unsigned(slot_operand_tag[i]) > sqTag) {
				slot_operand_busy[i] <= 0;
				slot_operand_q[i] <= 0;
			}
		}
	}
	// 2. paired push (CPU.cpp:286-353): each slot set finds its own free slot
	if (addr_push.valid && squash.valid == 0) {
		for (int i = 0; i < STORERS_CAP; ++i) {
			if (slot_busy[i] == 0) {
				slot_busy[i] <= 1;
				slot_op[i] <= to_unsigned(addr_push.op);
				slot_vj[i] <= to_unsigned(addr_push.vj);
				slot_qj[i] <= to_unsigned(addr_push.qj);
				slot_vk[i] <= to_unsigned(addr_push.vk);
				slot_rob_tag[i] <= to_unsigned(addr_push.rob_tag);
				break;
			}
		}
	}
	if (operand_push.valid && squash.valid == 0) {
		for (int i = 0; i < STORERS_CAP; ++i) {
			if (slot_operand_busy[i] == 0) {
				slot_operand_busy[i] <= 1;
				slot_operand_value[i] <= to_unsigned(operand_push.value);
				slot_operand_q[i] <= to_unsigned(operand_push.q);
				slot_operand_tag[i] <= to_unsigned(operand_push.rob_tag);
				break;
			}
		}
	}
	// 3. CDB broadcast: address results do not broadcast (CPU.cpp:870-873)
	if (cdb.valid && cdb.is_address == 0) {
		auto value = static_cast<bool>(cdb.is_control) ? to_unsigned(cdb.rob_value)
													   : to_unsigned(cdb.value);
		auto tag = to_unsigned(cdb.tag);
		for (int i = 0; i < STORERS_CAP; ++i) {
			// the addr slot and the operand slot are paired by index but carry
			// different tags: gate each consumption by its own slot's tag
			if (slot_busy[i]
				&& (squash.valid == 0
					|| to_unsigned(slot_rob_tag[i]) <= to_unsigned(squash.tag))
				&& to_unsigned(slot_qj[i]) == tag) {
				slot_vj[i] <= value;
				slot_qj[i] <= 0;
			}
			if (slot_operand_busy[i]
				&& (squash.valid == 0
					|| to_unsigned(slot_operand_tag[i])
						<= to_unsigned(squash.tag))
				&& to_unsigned(slot_operand_q[i]) == tag) {
				slot_operand_value[i] <= value;
				slot_operand_q[i] <= 0;
			}
		}
	}
	// 4. exec release: address slot accepted by ALU (EXEC_SEL decided, src=2)
	if (exec.valid && to_unsigned(exec.src) == 2) {
		auto idx = to_unsigned(exec.index);
		if (idx < static_cast<max_size_t>(STORERS_CAP) && slot_busy[idx]) {
			slot_busy[idx] <= 0;
			slot_qj[idx] <= 0;
		}
	}
	// 5. operand delivery: ALL ready slots delivered this cycle (CPU.cpp:644-659)
	for (int i = 0; i < STORERS_CAP; ++i) {
		if (operandReadyValid(i)) {
			slot_operand_busy[i] <= 0;
			slot_operand_q[i] <= 0;
		}
	}
}

} // namespace dark
