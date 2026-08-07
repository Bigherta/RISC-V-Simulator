#include "INQ.hpp"
#include "Decoder.hpp"
#include "RAT.hpp"
#include "REG.hpp"
#include "ROB.hpp"
#include "INT_RS.hpp"
#include "LOAD_RS.hpp"
#include "STORE_RS.hpp"
#include "BRANCH_RS.hpp"
#include "EXEC_SEL.hpp"
#include "CDB_Arb.hpp"
#include "ALU.hpp"
#include "LSQ.hpp"
#include "MEM.hpp"
#include "PC_Reg.hpp"
#include "BPU.hpp"
#include "BRU.hpp"
#include "FLUSH_ARB.hpp"

#include <iostream>

using namespace dark;

int main() {
	MEM_Backing backing;
	IMEM imem(backing);
	DMEM dmem(backing);
	PC_Reg pc_reg;
	BPU bpu;
	INQ inq;
	DEC dec;
	RAT rat;
	REG reg;
	ROB rob;
	INT_RS int_rs;
	LOAD_RS load_rs;
	STORE_RS store_rs;
	BRANCH_RS branch_rs;
	EXEC_SEL exec_sel;
	ALU alu;
	CDB_Arb cdb_arb;
	LSQ lsq;
	BRU bru;
	FLUSH_ARB flusher;
	dark::CPU cpu;

	cpu.add_module(&imem);
	cpu.add_module(&dmem);
	cpu.add_module(&pc_reg);
	cpu.add_module(&bpu);
	cpu.add_module(&inq);
	cpu.add_module(&dec);
	cpu.add_module(&rat);
	cpu.add_module(&reg);
	cpu.add_module(&rob);
	cpu.add_module(&int_rs);
	cpu.add_module(&load_rs);
	cpu.add_module(&store_rs);
	cpu.add_module(&branch_rs);
	cpu.add_module(&exec_sel);
	cpu.add_module(&alu);
	cpu.add_module(&cdb_arb);
	cpu.add_module(&lsq);
	cpu.add_module(&bru);
	cpu.add_module(&flusher);
	rat.rob_ptr = &rob;

	// ---- PC_Reg ----
	pc_reg.squash_valid = [&]() -> auto & { return flusher.valid; };
	pc_reg.squash_pc = [&]() -> auto & { return flusher.pc; };
	pc_reg.advance = [&]() { return imem.fetchingValid(); };
	pc_reg.next_pc = [&]() { return imem.computeNextPC(); };
	// ---- BPU <- PC / IMEM fetch ----
	bpu.pc = [&]() { return pc_reg.getPC(); };
	bpu.fetch_valid = [&]() { return imem.fetchingValid(); };
	bpu.fetch_raw = [&]() { return imem.computeFetchRaw(); };
	// ---- BPU <- branch resolution (BRU path, CPU.cpp:842-843) ----
	bpu.br.valid = [&]() { return !bru.isEmpty(); };
	bpu.br.tag = [&]() { return bru.peekTag(); };
	bpu.br.pc = [&]() { return bru.peekPCFrom(); };
	bpu.br.taken = [&]() { return bru.peekPCResult() != bru.peekPCFrom() + 4; };
	bpu.br.target = [&]() { return bru.peekPCResult(); };
	bpu.br.ghr = [&]() {
		int i = rob.getIndex(bru.peekTag());
		return i >= 0 ? static_cast<max_size_t>(rob.getCkpt(i) & 0xFFFFu) : 0;
	};
	// ---- BPU <- JALR resolution (CDB path, CPU.cpp:903-904) ----
	bpu.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	bpu.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	bpu.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	bpu.cdb.pc = [&]() {
		int i = rob.getIndex(cdb_arb.resultTag());
		return i >= 0 ? rob.getPC(i) : 0;
	};
	bpu.cdb.target = [&]() { return cdb_arb.resultValue(); };
	bpu.cdb.ghr = [&]() {
		int i = rob.getIndex(cdb_arb.resultTag());
		return i >= 0 ? static_cast<max_size_t>(rob.getCkpt(i) & 0xFFFFu) : 0;
	};
	// ---- BPU <- squash recover (CPU.cpp:1042-1045) ----
	bpu.squash.valid = [&]() -> auto & { return flusher.valid; };
	bpu.squash.tag = [&]() -> auto & { return flusher.tag; };
	bpu.recover_ok = [&]() { return rob.getIndex(to_unsigned(flusher.tag)) >= 0; };
	bpu.recover_ckpt = [&]() {
		int i = rob.getIndex(to_unsigned(flusher.tag));
		return i >= 0 ? rob.getCkpt(i) : 0;
	};
	// ---- IMEM <- BPU prediction (combined getters, same-cycle) ----
	imem.pc = [&]() { return pc_reg.getPC(); };
	imem.inq_full = [&]() { return inq.isFull(); };
	imem.squash_valid = [&]() -> auto & { return flusher.valid; };
	imem.pred_taken = [&]() {
		return bpu.predict(static_cast<int32_t>(pc_reg.getPC())).taken;
	};
	imem.pred_target = [&]() {
		return static_cast<max_size_t>(
			bpu.predict(static_cast<int32_t>(pc_reg.getPC())).predictPC);
	};
	imem.pred_ckpt = [&]() {
		auto c = bpu.snapshotCheckPoint();
		return (static_cast<max_size_t>(c.top) << 16) | (c.GHR & 0xFFFFu);
	};
	imem.ras_empty = [&]() { return bpu.RAS_empty(); };
	imem.ras_pop_value = [&]() { return bpu.rasTopValue(); };
	// ---- INQ ----
	inq.pop_valid = [&]() { return dec.consumingValid(); };
	inq.squash.valid = [&]() -> auto & { return flusher.valid; };
	inq.push_req.valid = [&]() -> auto & { return imem.push_valid; };
	inq.push_req.raw = [&]() -> auto & { return imem.push_raw; };
	inq.push_req.pc = [&]() -> auto & { return imem.push_pc; };
	inq.push_req.predpc = [&]() -> auto & { return imem.push_predpc; };
	inq.push_req.ckpt = [&]() -> auto & { return imem.push_ckpt; };
	// ---- DEC ----
	dec.inq_valid = [&]() { return inq.headValid(); };
	dec.inq_raw = [&]() { return inq.headRaw(); };
	dec.inq_pc = [&]() { return inq.headPC(); };
	dec.inq_predpc = [&]() { return inq.headPredPC(); };
	dec.inq_isHalt = [&]() { return inq.headIsHalt(); };
	dec.inq_ckpt = [&]() { return inq.headCkpt(); };
	dec.pop_dec_valid = [&]() { return rat.popDecValid(); };
	dec.squash_valid = [&]() -> auto & { return flusher.valid; };
	// ---- RAT <- DEC ----
	rat.dec.valid = [&]() { return dec.headValid(); };
	rat.dec.isHalt = [&]() { return dec.headIsHalt(); };
	rat.dec.type = [&]() { return dec.headType(); };
	rat.dec.opcode = [&]() { return dec.headOpcode(); };
	rat.dec.funct3 = [&]() { return dec.headFunct3(); };
	rat.dec.funct7 = [&]() { return dec.headFunct7(); };
	rat.dec.rd = [&]() { return dec.headRd(); };
	rat.dec.rs1 = [&]() { return dec.headRs1(); };
	rat.dec.rs2 = [&]() { return dec.headRs2(); };
	rat.dec.imm = [&]() { return dec.headImm(); };
	rat.dec.pc = [&]() { return dec.headPC(); };
	rat.dec.predpc = [&]() { return dec.headPredPC(); };
	rat.dec.ckpt = [&]() { return dec.headCkpt(); };
	// ---- RAT <- ROB / REG / CDB ----
	rat.rob.full = [&]() { return rob.isFull(); };
	rat.rob.rob_tag = [&]() -> auto & { return rob.rob_tag; };
	rat.rob.head_commit_ready = [&]() { return rob.headCommitReady(); };
	rat.rob.head_dest = [&]() { return rob.headDest(); };
	rat.rob.head_tag = [&]() { return rob.headTag(); };
	rat.rob.head_type = [&]() { return static_cast<max_size_t>(rob.headType()); };
	for (int i = 0; i < REGISTER_CAP; ++i) {
		rat.regfile.regs[i] = [&, i]() -> auto & { return reg.regs[i]; };
	}
	rat.robq.rs1_ready = [&]() {
		int idx = rob.getIndex(rat.rs1Tag());
		if (idx < 0) {
			return true; // tag committed: the value lives in the regfile
		}
		return rob.getValueReady(idx);
	};
	rat.robq.rs1_value = [&]() {
		int idx = rob.getIndex(rat.rs1Tag());
		if (idx < 0) {
			return reg.reg_ref(static_cast<int>(to_unsigned(rat.dec.rs1)));
		}
		return rob.getValue(idx);
	};
	rat.robq.rs2_ready = [&]() {
		int idx = rob.getIndex(rat.rs2Tag());
		if (idx < 0) {
			return true; // tag committed: the value lives in the regfile
		}
		return rob.getValueReady(idx);
	};
	rat.robq.rs2_value = [&]() {
		int idx = rob.getIndex(rat.rs2Tag());
		if (idx < 0) {
			return reg.reg_ref(static_cast<int>(to_unsigned(rat.dec.rs2)));
		}
		return rob.getValue(idx);
	};
	rat.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	rat.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	rat.cdb.value = [&]() { return cdb_arb.resultValue(); };
	rat.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	rat.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	// ---- RAT <- stalls ----
	rat.stall.rs_int_full = [&]() { return int_rs.isFull(); };
	rat.stall.rs_load_full = [&]() { return load_rs.isFull(); };
	rat.stall.rs_store_full = [&]() { return store_rs.isFull(); };
	rat.stall.rs_micro_full = [&]() { return store_rs.isOperandFull(); };
	rat.stall.rs_branch_full = [&]() { return branch_rs.isFull(); };
	rat.stall.lsq_full = [&]() { return lsq.isFull(); };
	rat.squash.valid = [&]() -> auto & { return flusher.valid; };
	rat.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- INT_RS ----
	int_rs.push.valid = [&]() { return rat.intPushValid(); };
	int_rs.push.op = [&]() { return rat.intPushOp(); };
	int_rs.push.vj = [&]() { return rat.intPushVj(); };
	int_rs.push.qj = [&]() { return rat.intPushQj(); };
	int_rs.push.vk = [&]() { return rat.intPushVk(); };
	int_rs.push.qk = [&]() { return rat.intPushQk(); };
	int_rs.push.rob_tag = [&]() { return rat.intPushRobTag(); };
	int_rs.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	int_rs.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	int_rs.cdb.value = [&]() { return cdb_arb.resultValue(); };
	int_rs.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	int_rs.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	int_rs.cdb.rob_value = [&]() {
		int idx = rob.getIndex(cdb_arb.resultTag());
		return idx >= 0 ? rob.getValue(idx) : 0;
	};
	int_rs.exec.valid = [&]() { return exec_sel.execValid(); };
	int_rs.exec.src = [&]() { return exec_sel.execSrc(); };
	int_rs.exec.index = [&]() { return exec_sel.execIndex(); };
	int_rs.squash.valid = [&]() -> auto & { return flusher.valid; };
	int_rs.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- LOAD_RS ----
	load_rs.push.valid = [&]() { return rat.loadPushValid(); };
	load_rs.push.op = [&]() { return rat.loadPushOp(); };
	load_rs.push.vj = [&]() { return rat.loadPushVj(); };
	load_rs.push.qj = [&]() { return rat.loadPushQj(); };
	load_rs.push.vk = [&]() { return rat.loadPushVk(); };
	load_rs.push.qk = [&]() { return rat.loadPushQk(); };
	load_rs.push.rob_tag = [&]() { return rat.loadPushRobTag(); };
	load_rs.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	load_rs.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	load_rs.cdb.value = [&]() { return cdb_arb.resultValue(); };
	load_rs.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	load_rs.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	load_rs.cdb.rob_value = [&]() {
		int idx = rob.getIndex(cdb_arb.resultTag());
		return idx >= 0 ? rob.getValue(idx) : 0;
	};
	load_rs.exec.valid = [&]() { return exec_sel.execValid(); };
	load_rs.exec.src = [&]() { return exec_sel.execSrc(); };
	load_rs.exec.index = [&]() { return exec_sel.execIndex(); };
	load_rs.squash.valid = [&]() -> auto & { return flusher.valid; };
	load_rs.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- STORE_RS ----
	store_rs.addr_push.valid = [&]() { return rat.storeAddrValid(); };
	store_rs.addr_push.op = [&]() { return rat.storeAddrOp(); };
	store_rs.addr_push.vj = [&]() { return rat.storeAddrVj(); };
	store_rs.addr_push.qj = [&]() { return rat.storeAddrQj(); };
	store_rs.addr_push.vk = [&]() { return rat.storeAddrVk(); };
	store_rs.addr_push.rob_tag = [&]() { return rat.storeAddrRobTag(); };
	store_rs.operand_push.valid = [&]() { return rat.storeOperandValid(); };
	store_rs.operand_push.value = [&]() { return rat.storeOperandValue(); };
	store_rs.operand_push.q = [&]() { return rat.storeOperandQ(); };
	store_rs.operand_push.rob_tag = [&]() { return rat.storeOperandRobTag(); };
	store_rs.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	store_rs.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	store_rs.cdb.value = [&]() { return cdb_arb.resultValue(); };
	store_rs.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	store_rs.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	store_rs.cdb.rob_value = [&]() {
		int idx = rob.getIndex(cdb_arb.resultTag());
		return idx >= 0 ? rob.getValue(idx) : 0;
	};
	store_rs.exec.valid = [&]() { return exec_sel.execValid(); };
	store_rs.exec.src = [&]() { return exec_sel.execSrc(); };
	store_rs.exec.index = [&]() { return exec_sel.execIndex(); };
	store_rs.squash.valid = [&]() -> auto & { return flusher.valid; };
	store_rs.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- BRANCH_RS ----
	branch_rs.push.valid = [&]() { return rat.branchPushValid(); };
	branch_rs.push.op = [&]() { return rat.branchPushOp(); };
	branch_rs.push.vj = [&]() { return rat.branchPushVj(); };
	branch_rs.push.qj = [&]() { return rat.branchPushQj(); };
	branch_rs.push.vk = [&]() { return rat.branchPushVk(); };
	branch_rs.push.qk = [&]() { return rat.branchPushQk(); };
	branch_rs.push.rob_tag = [&]() { return rat.branchPushRobTag(); };
	branch_rs.push.imm = [&]() { return rat.branchPushImm(); };
	branch_rs.push.pc = [&]() { return rat.branchPushPC(); };
	branch_rs.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	branch_rs.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	branch_rs.cdb.value = [&]() { return cdb_arb.resultValue(); };
	branch_rs.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	branch_rs.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	branch_rs.cdb.rob_value = [&]() {
		int idx = rob.getIndex(cdb_arb.resultTag());
		return idx >= 0 ? rob.getValue(idx) : 0;
	};
	branch_rs.bru_accept_valid = [&]() { return bru.acceptValid(); };
	branch_rs.bru_accept_index = [&]() { return bru.acceptIndex(); };
	branch_rs.squash.valid = [&]() -> auto & { return flusher.valid; };
	branch_rs.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- EXEC_SEL ----
	exec_sel.alu_full = [&]() { return alu.isFull(); };
	exec_sel.int_c.valid = [&]() { return int_rs.execCandidateValid(); };
	exec_sel.int_c.op = [&]() { return int_rs.execCandidateOp(); };
	exec_sel.int_c.vj = [&]() { return int_rs.execCandidateVj(); };
	exec_sel.int_c.vk = [&]() { return int_rs.execCandidateVk(); };
	exec_sel.int_c.tag = [&]() { return int_rs.execCandidateTag(); };
	exec_sel.int_c.index = [&]() { return int_rs.execCandidateIndex(); };
	exec_sel.load_c.valid = [&]() { return load_rs.execCandidateValid(); };
	exec_sel.load_c.op = [&]() { return load_rs.execCandidateOp(); };
	exec_sel.load_c.vj = [&]() { return load_rs.execCandidateVj(); };
	exec_sel.load_c.vk = [&]() { return load_rs.execCandidateVk(); };
	exec_sel.load_c.tag = [&]() { return load_rs.execCandidateTag(); };
	exec_sel.load_c.index = [&]() { return load_rs.execCandidateIndex(); };
	exec_sel.store_c.valid = [&]() { return store_rs.execCandidateValid(); };
	exec_sel.store_c.op = [&]() { return store_rs.execCandidateOp(); };
	exec_sel.store_c.vj = [&]() { return store_rs.execCandidateVj(); };
	exec_sel.store_c.vk = [&]() { return store_rs.execCandidateVk(); };
	exec_sel.store_c.tag = [&]() { return store_rs.execCandidateTag(); };
	exec_sel.store_c.index = [&]() { return store_rs.execCandidateIndex(); };
	// ---- ALU ----
	alu.rs_valid = [&]() { return exec_sel.execValid(); };
	alu.rs_op = [&]() { return exec_sel.execOp(); };
	alu.rs_vj = [&]() { return exec_sel.execVj(); };
	alu.rs_vk = [&]() { return exec_sel.execVk(); };
	alu.rs_tag = [&]() { return exec_sel.execTag(); };
	alu.squash_valid = [&]() -> auto & { return flusher.valid; };
	alu.squash_tag = [&]() -> auto & { return flusher.tag; };
	alu.cdb_valid = [&]() { return cdb_arb.aluGranted(); };
	alu.cdb_tag = [&]() { return cdb_arb.resultTag(); };
	// ---- CDB_Arb ----
	cdb_arb.alu_valid = [&]() { return !alu.isEmpty(); };
	cdb_arb.alu_tag = [&]() { return alu.peekTag(); };
	cdb_arb.alu_value = [&]() { return alu.peekValue(); };
	cdb_arb.alu_is_address = [&]() { return alu.peekIsAddress(); };
	cdb_arb.alu_is_control = [&]() { return alu.peekIsControl(); };
	cdb_arb.lsq_valid = [&]() { return lsq.lsqReadyIndex() >= 0; };
	cdb_arb.lsq_tag = [&]() {
		int i = lsq.lsqReadyIndex();
		return i >= 0 ? lsq.getTag(i) : 0;
	};
	cdb_arb.lsq_value = [&]() {
		int i = lsq.lsqReadyIndex();
		return i >= 0 ? lsq.getValue(i) : 0;
	};
	cdb_arb.lsq_index = [&]() {
		int i = lsq.lsqReadyIndex();
		return i >= 0 ? static_cast<max_size_t>(i) : 0;
	};
	cdb_arb.squash_valid = [&]() -> auto & { return flusher.valid; };
	cdb_arb.squash_tag = [&]() -> auto & { return flusher.tag; };
	// ---- ROB ----
	rob.exec.valid = [&]() { return cdb_arb.resultValid(); };
	rob.exec.value = [&]() { return cdb_arb.resultValue(); };
	rob.exec.tag = [&]() { return cdb_arb.resultTag(); };
	rob.exec.isAddress = [&]() { return cdb_arb.resultIsAddress(); };
	rob.exec.isControl = [&]() { return cdb_arb.resultIsControl(); };
	rob.bru.valid = [&]() { return !bru.isEmpty(); };
	rob.bru.value = [&]() { return bru.peekPCResult(); };
	rob.bru.tag = [&]() { return bru.peekTag(); };
	rob.bru.isAddress = []() { return 0u; };
	rob.bru.isControl = []() { return 0u; };
	rob.push_req.valid = [&]() { return rat.computePushValid(); };
	rob.push_req.meta = [&]() { return rat.computePushMeta(); };
	rob.push_req.pc = [&]() { return rat.computePushPC(); };
	rob.push_req.predpc = [&]() { return rat.computePushPredPC(); };
	rob.push_req.value = [&]() { return rat.computePushValue(); };
	rob.push_req.value_ready = [&]() { return rat.computePushVR(); };
	rob.push_req.commit_ready = [&]() { return rat.computePushCR(); };
	rob.push_req.ckpt = [&]() { return rat.computePushCkpt(); };
	rob.query.valid = []() { return 0u; };
	rob.query.tag = []() { return 0u; };
	rob.squash.valid = [&]() -> auto & { return flusher.valid; };
	rob.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- REG ----
	reg.head_commit_ready = [&]() { return rob.headCommitReady(); };
	reg.head_halt = [&]() { return rob.headHalt(); };
	reg.head_dest = [&]() { return rob.headDest(); };
	reg.head_value = [&]() { return rob.headValue(); };
	reg.head_type = [&]() { return static_cast<max_size_t>(rob.headType()); };
	// ---- LSQ ----
	lsq.push.valid = [&]() { return rat.lsqPushValid(); };
	lsq.push.is_load = [&]() { return rat.lsqPushIsLoad(); };
	lsq.push.n_bytes = [&]() { return rat.lsqPushNBytes(); };
	lsq.push.is_unsigned = [&]() { return rat.lsqPushIsUnsigned(); };
	lsq.push.rob_tag = [&]() { return rat.lsqPushRobTag(); };
	lsq.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	lsq.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	lsq.cdb.value = [&]() { return cdb_arb.resultValue(); };
	lsq.cdb.is_address = [&]() { return cdb_arb.resultIsAddress(); };
	lsq.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	for (int k = 0; k < STORERS_CAP; ++k) {
		lsq.operand[k].valid = [&, k]() { return store_rs.operandReadyValid(k); };
		lsq.operand[k].value = [&, k]() { return store_rs.operandReadyValue(k); };
		lsq.operand[k].tag = [&, k]() { return store_rs.operandReadyTag(k); };
	}
	lsq.mem_reply.valid = [&]() -> auto & { return dmem.reply_valid; };
	lsq.mem_reply.value = [&]() -> auto & { return dmem.reply_value; };
	lsq.mem_reply.rob_tag = [&]() -> auto & { return dmem.reply_rob_tag; };
	lsq.rob.store_dispatch_ok = [&]() {
		int i = rob.getIndex(lsq.headStoreTag());
		if (i == -1) {
			return true;
		}
		return i == static_cast<int>(to_unsigned(rob.head)) && rob.headCommitReady();
	};
	lsq.rob.head_load_absent = [&]() { return rob.getIndex(lsq.headLoadTag()) == -1; };
	lsq.cdb_lsq_granted = [&]() { return cdb_arb.lsqGranted(); };
	lsq.cdb_lsq_index = [&]() { return cdb_arb.lsqIndex(); };
	lsq.mem_busy = [&]() { return dmem.isBusyOrReq(); };
	lsq.squash.valid = [&]() -> auto & { return flusher.valid; };
	lsq.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- DMEM ----
	dmem.req.valid = [&]() -> auto & { return lsq.mem_request.valid; };
	dmem.req.is_load = [&]() -> auto & { return lsq.mem_request.is_load; };
	dmem.req.address = [&]() -> auto & { return lsq.mem_request.address; };
	dmem.req.value = [&]() -> auto & { return lsq.mem_request.value; };
	dmem.req.n_bytes = [&]() -> auto & { return lsq.mem_request.n_bytes; };
	dmem.req.is_signed = [&]() -> auto & { return lsq.mem_request.is_signed; };
	dmem.req.rob_tag = [&]() -> auto & { return lsq.mem_request.rob_tag; };
	// ---- BRU ----
	bru.cand_valid = [&]() { return branch_rs.execCandidateValid(); };
	bru.cand_op = [&]() { return branch_rs.execCandidateOp(); };
	bru.cand_vj = [&]() { return branch_rs.execCandidateVj(); };
	bru.cand_vk = [&]() { return branch_rs.execCandidateVk(); };
	bru.cand_pc = [&]() { return branch_rs.execCandidatePC(); };
	bru.cand_imm = [&]() { return branch_rs.execCandidateImm(); };
	bru.cand_tag = [&]() { return branch_rs.execCandidateTag(); };
	bru.cand_index = [&]() { return branch_rs.execCandidateIndex(); };
	bru.cdb_valid = [&]() { return !bru.isEmpty(); };
	bru.cdb_tag = [&]() { return bru.peekTag(); };
	bru.squash.valid = [&]() -> auto & { return flusher.valid; };
	bru.squash.tag = [&]() -> auto & { return flusher.tag; };
	// ---- FLUSH_ARB: BRU vs ROB predpc (CPU.cpp:822-845) ----
	flusher.branch.valid = [&]() { return !bru.isEmpty(); };
	flusher.branch.tag = [&]() { return bru.peekTag(); };
	flusher.branch.pc_result = [&]() { return bru.peekPCResult(); };
	flusher.branch.rob_ok = [&]() {
		int i = rob.getIndex(bru.peekTag());
		return i >= 0;
	};
	flusher.branch.rob_predpc = [&]() {
		int i = rob.getIndex(bru.peekTag());
		return i >= 0 ? rob.getPredPC(i) : 0;
	};
	// ---- FLUSH_ARB: JALR vs ROB predpc (CPU.cpp:890-909) ----
	flusher.cdb.valid = [&]() { return cdb_arb.resultValid(); };
	flusher.cdb.is_control = [&]() { return cdb_arb.resultIsControl(); };
	flusher.cdb.tag = [&]() { return cdb_arb.resultTag(); };
	flusher.cdb.pc = [&]() { return cdb_arb.resultValue(); };
	flusher.cdb.rob_ok = [&]() {
		int i = rob.getIndex(cdb_arb.resultTag());
		return i >= 0;
	};
	flusher.cdb.rob_predpc = [&]() {
		int i = rob.getIndex(cdb_arb.resultTag());
		return i >= 0 ? rob.getPredPC(i) : 0;
	};

	imem.load_from_stdin();

	bool halt_seen = false;
	unsigned long long guard = 0;
	while (!(halt_seen && rob.isEmpty() && inq.isEmpty() && dec.isEmpty()
			 && imem.getHaltFetched() && dmem.isBusyOrReq() == 0)
		   && guard++ < 200000000) {

		cpu.run_once();
		if (static_cast<max_size_t>(rob.commit_info.halt) != 0u) {
			halt_seen = true;
		}
	}
	if (!halt_seen || !rob.isEmpty()) {
		return 1;
	}
	// ports CPU::run (CPU.cpp:1071): x10 & 0xFF to stdout
	std::cout << std::dec << (reg.reg_ref(10) & 0xFFu) << std::endl;
	return 0;
}
