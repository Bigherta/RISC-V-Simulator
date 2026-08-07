#pragma once
#ifndef STORE_RS_HPP
#define STORE_RS_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct StoreAddrPush { // address calc slot: vj(base) + vk(imm), no qk
	Wire<1> valid;
	Wire<6> op;
	Wire<32> vj;
	Wire<31> qj;
	Wire<32> vk;
	Wire<31> rob_tag;
};

struct StoreOperandPush { // store data slot: rs2 value to be written to memory
	Wire<1> valid;
	Wire<32> value;
	Wire<31> q;
	Wire<31> rob_tag;
};

struct STORE_RS_Input {
	StoreAddrPush addr_push;
	StoreOperandPush operand_push;
	CDBIn cdb; // qj (address) + operand q (data)
	ExecAccept exec; // src = 2 (address slot release)
	SquashReq squash;
};

struct STORE_RS_Output {
	std::array<Register<6>, STORERS_CAP> slot_op; // address slots
	std::array<Register<32>, STORERS_CAP> slot_vj;
	std::array<Register<32>, STORERS_CAP> slot_vk;
	std::array<Register<31>, STORERS_CAP> slot_qj;
	std::array<Register<31>, STORERS_CAP> slot_rob_tag;
	std::array<Register<1>, STORERS_CAP> slot_busy;
	std::array<Register<32>, STORERS_CAP> slot_operand_value; // operand slots
	std::array<Register<31>, STORERS_CAP> slot_operand_q;
	std::array<Register<31>, STORERS_CAP> slot_operand_tag;
	std::array<Register<1>, STORERS_CAP> slot_operand_busy;
};

struct STORE_RS : Module<STORE_RS_Input, STORE_RS_Output> {
	bool isFull() const;
	bool isEmpty() const;
	bool isOperandFull() const;
	bool isOperandEmpty() const;
	bool isBusy(int i) const;
	int getIndex(max_size_t tag) const;
	max_size_t getOp(int i) const;
	max_size_t getVj(int i) const;
	max_size_t getQj(int i) const;
	max_size_t getVk(int i) const;
	max_size_t getRobTag(int i) const;
	bool isOperandBusy(int i) const;
	int getOperandIndex(max_size_t tag) const;
	max_size_t getOperandValue(int i) const;
	max_size_t getOperandQ(int i) const;
	max_size_t getOperandTag(int i) const;
	// execute candidate (address slot, bound by EXEC_SEL store group)
	bool execCandidateValid() const;
	max_size_t execCandidateIndex() const;
	max_size_t execCandidateOp() const;
	max_size_t execCandidateVj() const;
	max_size_t execCandidateVk() const;
	max_size_t execCandidateTag() const;
	// operand delivery candidates (param getters, wired per-slot by LSQ)
	bool operandReadyValid(int i) const;
	max_size_t operandReadyValue(int i) const;
	max_size_t operandReadyTag(int i) const;
	void work() override;
};

} // namespace dark
#endif // STORE_RS_HPP
