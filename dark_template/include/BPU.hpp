#pragma once
#ifndef BPU_HPP
#define BPU_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// branch resolution source (BRU path, ports CPU.cpp:842-843)
struct BRUUpdate {
	Wire<1> valid; // !bru.isEmpty()
	Wire<31> tag;
	Wire<32> pc; // peekPCFrom
	Wire<1> taken; // pc_result != pc_from + 4
	Wire<32> target; // peekPCResult
	Wire<16> ghr; // rob ckpt low 16 bits (update uses GHR, CPU.cpp:843)
};
// JALR resolution source (CDB isControl path, ports CPU.cpp:903-904)
struct CDBUpdate {
	Wire<1> valid; // cdb_arb.resultValid()
	Wire<1> is_control;
	Wire<31> tag;
	Wire<32> pc; // rob entry pc of the control result
	Wire<32> target; // cdb result value
	Wire<16> ghr; // rob ckpt low 16 bits
};

struct BPU_Input {
	Wire<32> pc; // current fetch pc (bound to PC_Reg.getPC)
	Wire<1> fetch_valid; // fetch gated on (bound to IMEM.fetchingValid)
	Wire<32> fetch_raw; // instruction being fetched (bound to IMEM.computeFetchRaw)
	BRUUpdate br;
	CDBUpdate cdb;
	SquashReq squash;
	Wire<1> recover_ok; // rob.getIndex(squash.tag) >= 0
	Wire<32> recover_ckpt; // packed (ras_top<<16)|GHR of the squash entry
};

struct BPU_Private { // ports BranchPredictor (src/include/BPU.hpp + src/BPU/BPU.cpp)
	std::array<Register<2>, BHT_CAP> globalPHT; // 2-bit saturating counters
	std::array<Register<2>, BHT_CAP> localPHT;
	std::array<Register<2>, SELECTOR_CAP> selector;
	std::array<Register<32>, BTB_CAP> btb_actual;
	std::array<Register<32>, BTB_CAP> btb_target;
	std::array<Register<1>, BTB_CAP> btb_valid;
	std::array<Register<32>, RAS_CAP> ras;
	Register<8> ras_top;
	Register<16> GHR;
	Register<1> inited; // one-shot init flag (PHT/selector start at 1)
};

// branch predictor: predict/update/recover ports of src/BPU/BPU.cpp.
// predict()/snapshotCheckPoint() are const combined getters (wireable by IMEM);
// RAS/GHR fetch-time mutations run inside work() driven by the fetch wires.
struct BPU : Module<BPU_Input, EmptyOutput, BPU_Private> {
	PredictInfo predict(int32_t pc) const; // BPU.cpp:5-19
	BranchPredictorCkpt snapshotCheckPoint() const; // BPU.cpp:78-80
	bool RAS_empty() const; // BPU.cpp:75
	bool RAS_full() const; // BPU.cpp:76
	max_size_t rasTopValue() const; // RAS[ras_top-1] (JALR predpc override)
	void work() override;
};

} // namespace dark
#endif // BPU_HPP
