#pragma once
#ifndef EXEC_SEL_HPP
#define EXEC_SEL_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// per-source execute candidate (wired to one RS combined getters)
struct ExecCandidateGroup {
	Wire<1> valid;
	Wire<6> op;
	Wire<32> vj;
	Wire<32> vk;
	Wire<31> tag;
	Wire<6> index;
};

// M4a: int candidate; M4b-1: + load; M4b-2: + store (address slots)
struct EXEC_SEL_Input {
	Wire<1> alu_full; // ALU.isFull() - no candidate selected when full
	ExecCandidateGroup int_c;
	ExecCandidateGroup load_c;
	ExecCandidateGroup store_c;
};

// stateless combinational selector: smallest rob_tag candidate wins
struct EXEC_SEL : Module<EXEC_SEL_Input, EmptyOutput> {
	bool execValid() const;
	max_size_t execOp() const;
	max_size_t execVj() const;
	max_size_t execVk() const;
	max_size_t execTag() const;
	max_size_t execSrc() const; // 0 = int, 1 = load, 2 = store
	max_size_t execIndex() const;
	void work() override; // empty
};

} // namespace dark
#endif // EXEC_SEL_HPP
