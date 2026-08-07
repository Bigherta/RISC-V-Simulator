#pragma once
#ifndef INT_RS_HPP
#define INT_RS_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct IntPush { // bound to RAT intPush* combined getters (q == 0 means ready)
	Wire<1> valid;
	Wire<6> op;
	Wire<32> vj;
	Wire<31> qj;
	Wire<32> vk;
	Wire<31> qk;
	Wire<31> rob_tag;
};

struct INT_RS_Input {
	IntPush push;
	CDBIn cdb;
	ExecAccept exec; // src = 0
	SquashReq squash;
};

struct INT_RS_Output {
	std::array<Register<6>, INTEGERRS_CAP> slot_op;
	std::array<Register<32>, INTEGERRS_CAP> slot_vj;
	std::array<Register<32>, INTEGERRS_CAP> slot_vk;
	std::array<Register<31>, INTEGERRS_CAP> slot_qj;
	std::array<Register<31>, INTEGERRS_CAP> slot_qk;
	std::array<Register<31>, INTEGERRS_CAP> slot_rob_tag;
	std::array<Register<1>, INTEGERRS_CAP> slot_busy; // 1 = occupied (0-init = empty)
};

struct INT_RS : Module<INT_RS_Input, INT_RS_Output> {
	bool isFull() const;
	bool isEmpty() const;
	// per-index getters
	bool isBusy(int i) const;
	int getIndex(max_size_t tag) const;
	max_size_t getOp(int i) const;
	max_size_t getVj(int i) const;
	max_size_t getQj(int i) const;
	max_size_t getVk(int i) const;
	max_size_t getQk(int i) const;
	max_size_t getRobTag(int i) const;
	// execute candidate (combined, wire-bound by EXEC_SEL):
	// ready (!free && qj==0 && qk==0) and older than squash tag, smallest rob_tag wins
	bool execCandidateValid() const;
	max_size_t execCandidateIndex() const;
	max_size_t execCandidateOp() const;
	max_size_t execCandidateVj() const;
	max_size_t execCandidateVk() const;
	max_size_t execCandidateTag() const;
	void work() override;
};

} // namespace dark
#endif // INT_RS_HPP
