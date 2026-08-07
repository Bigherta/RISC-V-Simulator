#pragma once
#ifndef LOAD_RS_HPP
#define LOAD_RS_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct LoadPush { // bound to RAT loadPush* combined getters (vk = imm, qk = 0)
	Wire<1> valid;
	Wire<6> op;
	Wire<32> vj;
	Wire<31> qj;
	Wire<32> vk;
	Wire<31> qk;
	Wire<31> rob_tag;
};

struct LOAD_RS_Input {
	LoadPush push;
	CDBIn cdb;
	ExecAccept exec; // src = 1
	SquashReq squash;
};

struct LOAD_RS_Output {
	std::array<Register<6>, LOADRS_CAP> slot_op;
	std::array<Register<32>, LOADRS_CAP> slot_vj;
	std::array<Register<32>, LOADRS_CAP> slot_vk;
	std::array<Register<31>, LOADRS_CAP> slot_qj;
	std::array<Register<31>, LOADRS_CAP> slot_qk;
	std::array<Register<31>, LOADRS_CAP> slot_rob_tag;
	std::array<Register<1>, LOADRS_CAP> slot_busy;
};

struct LOAD_RS : Module<LOAD_RS_Input, LOAD_RS_Output> {
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
	// execute candidate (bound by EXEC_SEL load group)
	bool execCandidateValid() const;
	max_size_t execCandidateIndex() const;
	max_size_t execCandidateOp() const;
	max_size_t execCandidateVj() const;
	max_size_t execCandidateVk() const;
	max_size_t execCandidateTag() const;
	void work() override;
};

} // namespace dark
#endif // LOAD_RS_HPP
