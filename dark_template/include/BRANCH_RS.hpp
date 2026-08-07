#pragma once
#ifndef BRANCH_RS_HPP
#define BRANCH_RS_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct BranchPush { // bound to RAT branchPush* combined getters
	Wire<1> valid;
	Wire<6> op;
	Wire<32> vj;
	Wire<31> qj;
	Wire<32> vk;
	Wire<31> qk;
	Wire<31> rob_tag;
	Wire<32> imm;
	Wire<32> pc;
};

struct BRANCH_RS_Input {
	BranchPush push;
	CDBIn cdb;
	Wire<1> bru_accept_valid; // BRU.acceptValid() - slot released on accept
	Wire<6> bru_accept_index;
	SquashReq squash;
};

struct BRANCH_RS_Output {
	std::array<Register<6>, BRANCHRS_CAP> slot_op;
	std::array<Register<32>, BRANCHRS_CAP> slot_vj;
	std::array<Register<32>, BRANCHRS_CAP> slot_vk;
	std::array<Register<31>, BRANCHRS_CAP> slot_qj;
	std::array<Register<31>, BRANCHRS_CAP> slot_qk;
	std::array<Register<31>, BRANCHRS_CAP> slot_rob_tag;
	std::array<Register<1>, BRANCHRS_CAP> slot_busy;
	std::array<Register<32>, BRANCHRS_CAP> slot_imm;
	std::array<Register<32>, BRANCHRS_CAP> slot_pc;
};

// M4b-1: storage + CDB consumption only; release comes with BRU in M6
struct BRANCH_RS : Module<BRANCH_RS_Input, BRANCH_RS_Output> {
	bool isFull() const;
	bool isEmpty() const;
	bool isBusy(int i) const;
	int getIndex(max_size_t tag) const;
	max_size_t getOp(int i) const;
	max_size_t getVj(int i) const;
	max_size_t getQj(int i) const;
	max_size_t getVk(int i) const;
	max_size_t getQk(int i) const;
	max_size_t getRobTag(int i) const;
	max_size_t getImm(int i) const;
	max_size_t getPC(int i) const;
	// execute candidate (bound by BRU; released on accept)
	bool execCandidateValid() const;
	max_size_t execCandidateIndex() const;
	max_size_t execCandidateOp() const;
	max_size_t execCandidateVj() const;
	max_size_t execCandidateVk() const;
	max_size_t execCandidateTag() const;
	max_size_t execCandidatePC() const;
	max_size_t execCandidateImm() const;
	void work() override;
};

} // namespace dark
#endif // BRANCH_RS_HPP
