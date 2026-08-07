#pragma once
#ifndef DECODER_HPP
#define DECODER_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

class Decoder { // pure decode logic (ported from src/Decoder)
public:
	static Instruct decode(int32_t raw_inst);
	static inline int32_t signExtend(int32_t raw_data, int len) {
		return (raw_data << (32 - len)) >> (32 - len);
	}
};

struct DEC_Input { // INQ head (wire-bound) + issue decision + squash
	Wire<1> inq_valid;
	Wire<32> inq_raw;
	Wire<32> inq_pc;
	Wire<32> inq_predpc;
	Wire<1> inq_isHalt;
	Wire<32> inq_ckpt;
	Wire<1> pop_dec_valid; // issue consumed the decoded head (bound to RAT.popDecValid)
	Wire<1> squash_valid;
};

struct DEC_Output { // pop request (INQ consumes one entry this cycle)
	Register<1> pop_request;
};

struct DEC_Private { // decoded queue (DECODER_CAP)
	std::array<Register<32>, DECODER_CAP> slot_imm;
	std::array<Register<5>, DECODER_CAP> slot_rd;
	std::array<Register<5>, DECODER_CAP> slot_rs1;
	std::array<Register<5>, DECODER_CAP> slot_rs2;
	std::array<Register<3>, DECODER_CAP> slot_type; // RISC_V
	std::array<Register<7>, DECODER_CAP> slot_opcode;
	std::array<Register<10>, DECODER_CAP> slot_funct; // funct3(3)|funct7(7)
	std::array<Register<32>, DECODER_CAP> slot_pc;
	std::array<Register<32>, DECODER_CAP> slot_predpc;
	std::array<Register<32>, DECODER_CAP> slot_ckpt;
	std::array<Register<1>, DECODER_CAP> slot_isHalt;
	Register<5> head;
	Register<5> tail;
};

struct DEC : Module<DEC_Input, DEC_Output, DEC_Private> {
	bool isEmpty() const;
	bool isFull() const;
	bool consumingValid() const; // this cycle consumes INQ head (INQ pop decision)
	// head getters (no-arg, wire-bindable)
	bool headValid() const;
	bool headIsHalt() const;
	max_size_t headRd() const;
	max_size_t headRs1() const;
	max_size_t headRs2() const;
	max_size_t headImm() const;
	max_size_t headType() const;
	max_size_t headOpcode() const;
	max_size_t headFunct3() const;
	max_size_t headFunct7() const;
	max_size_t headPC() const;
	max_size_t headPredPC() const;
	max_size_t headCkpt() const;
	// write helpers (work() only)
	void pushEntry(Instruct inst, max_size_t pc, max_size_t predpc,
				   max_size_t ckpt, bool isHalt);
	void pop();
	void clear();
	void work() override;
};

} // namespace dark
#endif // DECODER_HPP
