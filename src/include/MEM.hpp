#pragma once
#ifndef MEM_HPP
#define MEM_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// MEM_SIZE from src/include/Memory.hpp:9
static constexpr uint32_t MEM_SIZE = 256 * 1024;

// shared backing memory (Modified Harvard: one address space, I/D views).
// Order-independence assumption: instruction region and data region never
// overlap within a cycle (riscv test layout); DMEM does at most one
// operation per cycle (LSQ single dispatch).
using MEM_Backing = std::array<uint8_t, MEM_SIZE>;

// ---------------------------------------------------------------------------
// IMEM: instruction fetch (ports CPU::fetch, CPU.cpp:9-54). The program
// counter lives in PC_Reg (separate register module); this module fetches
// from the pc Wire, predicts via BPU combined getters (wire-bound), and
// computes the next pc combinationally for PC_Reg.
// ---------------------------------------------------------------------------

struct IMEM_Input {
	Wire<32> pc; // bound to PC_Reg.getPC()
	Wire<1> inq_full; // INQ.isFull() - stop fetching when full
	Wire<1> squash_valid; // push gating + halt clear (PC reload is PC_Reg's job)
	Wire<1> pred_taken; // bpu.predict(pc).taken (combined, same-cycle)
	Wire<32> pred_target; // bpu.predict(pc).predictPC
	Wire<32> pred_ckpt; // packed snapshot (ras_top<<16)|GHR
	Wire<1> ras_empty; // bpu.RAS_empty()
	Wire<32> ras_pop_value; // bpu.rasTopValue() (JALR predpc override)
};

struct IMEM_Output { // fetch result -> INQ push_req (Register group, next cycle)
	Register<1> push_valid;
	Register<32> push_raw;
	Register<32> push_pc;
	Register<32> push_predpc;
	Register<32> push_ckpt;
};

struct IMEM_Private {
	Register<1> halt_fetched;
};

struct IMEM : Module<IMEM_Input, IMEM_Output, IMEM_Private> {
	MEM_Backing &mem; // shared backing view
	IMEM(MEM_Backing &m) : mem(m) {}
	void load_from_stdin(); // public entry -> private load_ins()
	// combined getters (wire-bound by PC_Reg / BPU, same old-state decision)
	bool fetchingValid() const; // fetch happens this cycle (PC_Reg.advance)
	max_size_t computeFetchRaw() const; // raw for the current fetch (BPU)
	max_size_t computeNextPC() const; // predicted next pc (PC_Reg.next_pc)
	bool getHaltFetched() const;
	void work() override;

private:
	max_size_t computePredPC() const; // shared by push_predpc / computeNextPC
	void load_ins(); // stdin hex stream loader (ports Memory::load_ins)
	static inline uint32_t hex2uint32(int len, char hex[]);
};

// ---------------------------------------------------------------------------
// DMEM: data memory (ports Memory.cpp, CPU.cpp DMEM usage)
// ---------------------------------------------------------------------------

struct DMEMReq { // bound to LSQ.mem_request (Register group)
	Wire<1> valid;
	Wire<1> is_load;
	Wire<32> address;
	Wire<32> value;
	Wire<3> n_bytes;
	Wire<1> is_signed;
	Wire<31> rob_tag;
};

struct DMEM_Input {
	DMEMReq req;
};

struct DMEM_Private { // execution state (MemExecution, Memory.cpp:16-17)
	Register<1> busy;
	Register<2> remain_cycle;
	Register<1> exec_is_load;
	Register<32> exec_address;
	Register<32> exec_value;
	Register<3> exec_n_bytes;
	Register<1> exec_is_signed;
	Register<31> exec_rob_tag;
};

struct DMEM_Output { // memory reply bus (work writes, LSQ reads next cycle)
	Register<1> reply_valid;
	Register<32> reply_value;
	Register<31> reply_rob_tag;
};

struct DMEM : Module<DMEM_Input, DMEM_Output, DMEM_Private> {
	MEM_Backing &mem; // shared backing view
	DMEM(MEM_Backing &m) : mem(m) {}
	int32_t load_n_bytes(uint32_t addr, int n, bool isSigned) const; // Memory.cpp:38-51
	void store_n_bytes(uint32_t addr, int value, int n); // Memory.cpp:53-58
	bool isBusyOrReq() const; // busy || req_valid (in-flight window for LSQ dispatch)
	bool isBusy() const;
	bool isReady() const;
	void work() override;
};

} // namespace dark
#endif // MEM_HPP
