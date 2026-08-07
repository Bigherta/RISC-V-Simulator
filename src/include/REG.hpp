#pragma once
#ifndef REG_HPP
#define REG_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct REG_Input { // ROB head state (wire-bound to ROB getters)
	Wire<1> head_commit_ready;
	Wire<1> head_halt; // halt commits without register writeback (CPU.cpp:945-951)
	Wire<5> head_dest;
	Wire<32> head_value;
	Wire<3> head_type; // ROBType
};

struct REG_Output {
	std::array<Register<32>, REGISTER_CAP> regs;
};

struct REG : Module<REG_Input, REG_Output> {
	max_size_t reg_ref(int i) const; // read own old (x0 always 0)
	void work() override;
};

} // namespace dark
#endif // REG_HPP
