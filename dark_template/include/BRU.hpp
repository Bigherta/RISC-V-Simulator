#pragma once
#ifndef BRU_HPP
#define BRU_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct BRU_Input { // candidate from BRANCH_RS execCandidate* (combined getters)
	Wire<1> cand_valid;
	Wire<6> cand_op;
	Wire<32> cand_vj;
	Wire<32> cand_vk;
	Wire<32> cand_pc;
	Wire<32> cand_imm;
	Wire<31> cand_tag;
	Wire<6> cand_index;
	Wire<1> cdb_valid; // result consumed (remove head)
	Wire<31> cdb_tag;
	SquashReq squash;
};

struct BRU_Output { // result queue (ports BRU outputBuffer, src/BRU/BRU.cpp)
	std::array<Register<1>, BRU_CAP> slot_valid;
	std::array<Register<32>, BRU_CAP> slot_pc_from;
	std::array<Register<32>, BRU_CAP> slot_pc_result;
	std::array<Register<31>, BRU_CAP> slot_rob_tag;
};

struct BRU : Module<BRU_Input, BRU_Output> {
	bool isEmpty() const;
	bool isFull() const;
	// queue head (smallest rob_tag) combined getters
	max_size_t peekTag() const;
	max_size_t peekPCFrom() const;
	max_size_t peekPCResult() const;
	// accept decision (for BRANCH_RS release): candidate && !full && squash filter
	bool acceptValid() const;
	max_size_t acceptIndex() const;
	void remove(max_size_t robTag);
	void flush(max_size_t flushTag);
	void work() override;
};

} // namespace dark
#endif // BRU_HPP
