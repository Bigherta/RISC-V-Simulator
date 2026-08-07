#include "../include/BRANCH_RS.hpp"

namespace dark {

bool BRANCH_RS::isFull() const {
	for (int i = 0; i < BRANCHRS_CAP; ++i) {
		if (slot_busy[i] == 0) {
			return false;
		}
	}
	return true;
}

bool BRANCH_RS::isEmpty() const {
	for (int i = 0; i < BRANCHRS_CAP; ++i) {
		if (slot_busy[i]) {
			return false;
		}
	}
	return true;
}

bool BRANCH_RS::isBusy(int i) const {
	return static_cast<bool>(slot_busy[i]);
}

int BRANCH_RS::getIndex(max_size_t tag) const {
	for (int i = 0; i < BRANCHRS_CAP; ++i) {
		if (slot_busy[i] && to_unsigned(slot_rob_tag[i]) == tag) {
			return i;
		}
	}
	return -1;
}

max_size_t BRANCH_RS::getOp(int i) const {
	return to_unsigned(slot_op[i]);
}

max_size_t BRANCH_RS::getVj(int i) const {
	return to_unsigned(slot_vj[i]);
}

max_size_t BRANCH_RS::getQj(int i) const {
	return to_unsigned(slot_qj[i]);
}

max_size_t BRANCH_RS::getVk(int i) const {
	return to_unsigned(slot_vk[i]);
}

max_size_t BRANCH_RS::getQk(int i) const {
	return to_unsigned(slot_qk[i]);
}

max_size_t BRANCH_RS::getRobTag(int i) const {
	return to_unsigned(slot_rob_tag[i]);
}

max_size_t BRANCH_RS::getImm(int i) const {
	return to_unsigned(slot_imm[i]);
}

max_size_t BRANCH_RS::getPC(int i) const {
	return to_unsigned(slot_pc[i]);
}

namespace {

// smallest rob_tag among ready slots; returns -1 if none
inline int branchCandidate(const BRANCH_RS &rs, bool withSquash, max_size_t squashTag) {
	int best = -1;
	max_size_t bestTag = 0;
	for (int i = 0; i < BRANCHRS_CAP; ++i) {
		if (rs.slot_busy[i] == 0) {
			continue;
		}
		auto tag = to_unsigned(rs.slot_rob_tag[i]);
		if (to_unsigned(rs.slot_qj[i]) != 0 || to_unsigned(rs.slot_qk[i]) != 0) {
			continue;
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

bool BRANCH_RS::execCandidateValid() const {
	return branchCandidate(*this, static_cast<bool>(squash.valid),
						   to_unsigned(squash.tag)) >= 0;
}

max_size_t BRANCH_RS::execCandidateIndex() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? static_cast<max_size_t>(i) : 0;
}

max_size_t BRANCH_RS::execCandidateOp() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_op[i]) : 0;
}

max_size_t BRANCH_RS::execCandidateVj() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_vj[i]) : 0;
}

max_size_t BRANCH_RS::execCandidateVk() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_vk[i]) : 0;
}

max_size_t BRANCH_RS::execCandidateTag() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_rob_tag[i]) : 0;
}

max_size_t BRANCH_RS::execCandidatePC() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_pc[i]) : 0;
}

max_size_t BRANCH_RS::execCandidateImm() const {
	int i = branchCandidate(*this, static_cast<bool>(squash.valid),
							 to_unsigned(squash.tag));
	return i >= 0 ? to_unsigned(slot_imm[i]) : 0;
}

void BRANCH_RS::work() {
	// 1. squash: free slots younger than squash tag (CPU.cpp:997-1003)
	if (squash.valid) {
		auto sqTag = to_unsigned(squash.tag);
		for (int i = 0; i < BRANCHRS_CAP; ++i) {
			if (slot_busy[i] && to_unsigned(slot_rob_tag[i]) > sqTag) {
				slot_busy[i] <= 0;
				slot_qj[i] <= 0;
				slot_qk[i] <= 0;
			}
		}
	}
	// 2. push (CPU.cpp:171-227)
	if (push.valid && squash.valid == 0) {
		for (int i = 0; i < BRANCHRS_CAP; ++i) {
			if (slot_busy[i] == 0) {
				slot_busy[i] <= 1;
				slot_op[i] <= to_unsigned(push.op);
				slot_vj[i] <= to_unsigned(push.vj);
				slot_qj[i] <= to_unsigned(push.qj);
				slot_vk[i] <= to_unsigned(push.vk);
				slot_qk[i] <= to_unsigned(push.qk);
				slot_rob_tag[i] <= to_unsigned(push.rob_tag);
				slot_imm[i] <= to_unsigned(push.imm);
				slot_pc[i] <= to_unsigned(push.pc);
				break;
			}
		}
	}
	// 3. CDB broadcast (CPU.cpp:784-793); address results do not broadcast
	if (cdb.valid && cdb.is_address == 0) {
		auto value = static_cast<bool>(cdb.is_control) ? to_unsigned(cdb.rob_value)
													   : to_unsigned(cdb.value);
		auto tag = to_unsigned(cdb.tag);
		for (int i = 0; i < BRANCHRS_CAP; ++i) {
			if (slot_busy[i] == 0) {
				continue;
			}
			if (squash.valid
				&& to_unsigned(slot_rob_tag[i]) > to_unsigned(squash.tag)) {
				continue; // slot is being flushed this cycle
			}
			if (to_unsigned(slot_qj[i]) == tag) {
				slot_vj[i] <= value;
				slot_qj[i] <= 0;
			}
			if (to_unsigned(slot_qk[i]) == tag) {
				slot_vk[i] <= value;
				slot_qk[i] <= 0;
			}
		}
	}
	// 4. BRU accept release (ports CPU.cpp:676-687: candidate consumed by BRU)
	if (bru_accept_valid) {
		auto idx = to_unsigned(bru_accept_index);
		if (idx < static_cast<max_size_t>(BRANCHRS_CAP) && slot_busy[idx]) {
			slot_busy[idx] <= 0;
			slot_qj[idx] <= 0;
			slot_qk[idx] <= 0;
		}
	}
}

} // namespace dark
