#pragma once
#ifndef PC_REG_HPP
#define PC_REG_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// program counter register module (ports CPUstate.programCounter, CPU.cpp:12)
struct PC_Reg_Input {
	Wire<1> squash_valid; // flush redirect (bound to FLUSH_ARB.valid)
	Wire<32> squash_pc;
	Wire<1> advance; // fetch happened this cycle (bound to IMEM.fetchingValid)
	Wire<32> next_pc; // predicted next pc (bound to IMEM.computeNextPC)
};

struct PC_Reg_Private {
	Register<32> pc;
};

struct PC_Reg : Module<PC_Reg_Input, PC_Reg_Private> {
	max_size_t getPC() const;
	void work() override;
};

} // namespace dark
#endif // PC_REG_HPP
