#pragma once
#ifndef RAT_HPP
#define RAT_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct DecHead { // bound to Decoder getters (decoded queue head)
	Wire<1> valid;
	Wire<1> isHalt;
	Wire<3> type; // RISC_V
	Wire<7> opcode;
	Wire<3> funct3;
	Wire<7> funct7;
	Wire<5> rd;
	Wire<5> rs1;
	Wire<5> rs2;
	Wire<32> imm;
	Wire<32> pc;
	Wire<32> predpc;
	Wire<32> ckpt;
};
struct RobState { // bound to ROB getters / Register ref
	Wire<1> full;
	Wire<31> rob_tag;
	Wire<1> head_commit_ready;
	Wire<5> head_dest;
	Wire<31> head_tag;
	Wire<3> head_type; // ROBType
};
struct StallFlags { // M3 stub / M4 bound to real RS/LSQ
	Wire<1> rs_int_full;
	Wire<1> rs_load_full;
	Wire<1> rs_store_full;
	Wire<1> rs_micro_full;
	Wire<1> rs_branch_full;
	Wire<1> lsq_full;
};
struct RegFile { // current register values (bound to REG.regs[i])
	std::array<Wire<32>, REGISTER_CAP> regs;
};
struct RobQuery { // issue-time operand resolution from ROB (wired via lambda)
	Wire<1> rs1_ready;
	Wire<32> rs1_value;
	Wire<1> rs2_ready;
	Wire<32> rs2_value;
};
// SquashReq is in common.hpp (shared by ROB/RAT/INQ)
// cdb: CDBIn from common.hpp (bypass resolution; first 5 fields used)

struct ROB; // forward decl: RAT::rob_ptr (squash repair traversal)

struct RAT_Input {
	DecHead dec;
	RobState rob;
	StallFlags stall;
	SquashReq squash;
	RegFile regfile;
	RobQuery robq;
	CDBIn cdb;
};

struct RAT_Private {
	std::array<Register<32>, 32> rat_table; // 0 = no rename (tag starts at 1)
};

struct RAT : Module<RAT_Input, RAT_Private> {
	const ROB *rob_ptr = nullptr; // squash repair traversal (raw member, not synced)
	// combined const methods (wire-bound by ROB/RS/INQ, decision-10 style)
	max_size_t ratRead(int reg) const; // own RAT table (test/debug read)
	bool computePushValid() const;
	max_size_t computePushMeta() const; // type(2)|dest(5)|halt(1)
	max_size_t computePushPC() const;
	max_size_t computePushPredPC() const;
	max_size_t computePushValue() const; // LINK: pc+4
	max_size_t computePushVR() const;
	max_size_t computePushCR() const;
	max_size_t computePushCkpt() const;
	bool popDecValid() const; // issue consumed the decoded head (Decoder pop decision)
	// rs1/rs2 resolution outputs for RS (M4, signature reserved)
	max_size_t rs1Tag() const; // RAT[rs1] or 0
	max_size_t rs2Tag() const;
	bool rs1HasTag() const;
	bool rs2HasTag() const;
	// M4a: Integer RS issue payload (operand-resolved, bound by INT_RS)
	bool intPushValid() const;
	max_size_t intPushOp() const;
	max_size_t intPushVj() const;
	max_size_t intPushQj() const;
	max_size_t intPushVk() const;
	max_size_t intPushQk() const;
	max_size_t intPushRobTag() const;
	// M4b-1: Load / Branch RS issue payloads
	bool loadPushValid() const;
	max_size_t loadPushOp() const;
	max_size_t loadPushVj() const;
	max_size_t loadPushQj() const;
	max_size_t loadPushVk() const;
	max_size_t loadPushQk() const;
	max_size_t loadPushRobTag() const;
	bool branchPushValid() const;
	max_size_t branchPushOp() const;
	max_size_t branchPushVj() const;
	max_size_t branchPushQj() const;
	max_size_t branchPushVk() const;
	max_size_t branchPushQk() const;
	max_size_t branchPushImm() const;
	max_size_t branchPushPC() const;
	max_size_t branchPushRobTag() const;
	// M4b-2: Store address / operand payloads (paired push)
	bool storeAddrValid() const;
	max_size_t storeAddrOp() const;
	max_size_t storeAddrVj() const;
	max_size_t storeAddrQj() const;
	max_size_t storeAddrVk() const;
	max_size_t storeAddrRobTag() const;
	bool storeOperandValid() const;
	max_size_t storeOperandValue() const;
	max_size_t storeOperandQ() const;
	max_size_t storeOperandRobTag() const;
	// M4b-3: LSQ push (same-cycle as issue)
	bool lsqPushValid() const;
	max_size_t lsqPushIsLoad() const;
	max_size_t lsqPushNBytes() const;
	max_size_t lsqPushIsUnsigned() const;
	max_size_t lsqPushRobTag() const;
	void work() override;
};

} // namespace dark
#endif // RAT_HPP
