#pragma once
#ifndef FLUSH_ARB_HPP
#define FLUSH_ARB_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// squash request sources (writeBack detection, ports CPU.cpp:821-922):
struct FlushBranchSrc { // BRU result path (ports CPU.cpp:822-845)
	Wire<1> valid; // !bru.isEmpty()
	Wire<31> tag; // bru.peekTag()
	Wire<32> pc_result; // bru.peekPCResult()
	Wire<1> rob_ok; // rob.getIndex(tag) >= 0 (wired via lambda)
	Wire<32> rob_predpc; // rob.getPredPC(index)
};
struct FlushCdbSrc { // JALR (isControl) CDB result path (ports CPU.cpp:890-909)
	Wire<1> valid; // cdb_arb.resultValid()
	Wire<1> is_control; // cdb_arb.resultIsControl()
	Wire<31> tag; // cdb_arb.resultTag()
	Wire<32> pc; // cdb_arb.resultValue()
	Wire<1> rob_ok;
	Wire<32> rob_predpc;
};

struct FLUSH_ARB_Input {
	FlushBranchSrc branch;
	FlushCdbSrc cdb;
};

struct FLUSH_ARB_Output { // active squash (all modules bind here)
	Register<1> valid;
	Register<31> tag;
	Register<32> pc;
};

struct FLUSH_ARB_Private { // sorted request buffer (ports FlushArbiter, src/include/Arbiter.hpp:30-81)
	std::array<Register<1>, FLUSHARBITER_CAP> req_valid;
	std::array<Register<31>, FLUSHARBITER_CAP> req_tag;
	std::array<Register<32>, FLUSHARBITER_CAP> req_pc;
};

// squash detection + arbitration (ports writeBack BranchSquash/JumpSquash +
// FlushArbiter receive/arbitResult/clear)
struct FLUSH_ARB : Module<FLUSH_ARB_Input, FLUSH_ARB_Output, FLUSH_ARB_Private> {
	void work() override;
};

} // namespace dark
#endif // FLUSH_ARB_HPP
