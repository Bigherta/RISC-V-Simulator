#pragma once
#ifndef ROB_HPP
#define ROB_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// ROBType is in common.hpp (shared by ROB/REG/RAT)

// packed form (debug/test only; production uses bit getters)
enum class ROBState : max_size_t {
	Waiting,
	ValueReady,
	CommitReady,
};

struct ROBEntry {
	ROBType type = ROBType::REGISTER;
	ROBState state = ROBState::Waiting;
	int tag = -1;
	int dest = -1;
	int32_t value = 0;
	uint32_t predictedPC = 0;
	int32_t pc = 0;
	bool halt = false;
	BranchPredictorCkpt ras_ckpt;
};

struct ExecReq { // execution result from ALU
	Wire<1> valid;
	Wire<32> value;
	Wire<31> tag;
	Wire<1> isAddress; // load/store address calc
	Wire<1> isControl; // JALR
};
struct QueryReq { // query request from RS
	Wire<1> valid;
	Wire<31> tag;
};
struct PushReq { // push request from RAT
	Wire<1> valid;
	Wire<8> meta; // type(2)|dest(5)|halt(1)
	Wire<32> pc;
	Wire<32> predpc;
	Wire<32> value; // LINK: pc+4
	Wire<1> value_ready; // LINK ready at push
	Wire<1> commit_ready; // halt ready at push
	Wire<32> ckpt; // packed top|GHR (23 bits)
};
// SquashReq is in common.hpp (shared by ROB/RAT/INQ)

struct ROB_Input {
	ExecReq exec;
	ExecReq bru; // branch result: BRANCH commit_ready (ports CPU.cpp:844-845)
	QueryReq query;
	PushReq push_req;
	SquashReq squash;
};

struct QueryReply { // query pipeline reply (written cycle N+1, visible N+2)
	Register<1> valid;
	Register<1> ready;
	Register<32> value;
};
struct CommitInfo { // last committed entry (written on pop, cleared otherwise)
	Register<1> halt;
	Register<5> dest;
};

struct ROB_Output {
	std::array<Register<32>, ROB_CAP> slot_value;
	std::array<Register<32>, ROB_CAP> slot_predpc;
	std::array<Register<32>, ROB_CAP> slot_pc;
	std::array<Register<8>, ROB_CAP> slot_meta;
	std::array<Register<31>, ROB_CAP> slot_tag;
	std::array<Register<1>, ROB_CAP> slot_value_ready;
	std::array<Register<1>, ROB_CAP> slot_commit_ready;
	std::array<Register<32>, ROB_CAP> slot_ckpt;
	Register<6> head;
	Register<6> tail;
	Register<31> rob_tag; // last allocated tag (push assigns rob_tag+1, first tag=1)
	QueryReply query_reply; // 组打包（规避 14 成员限制）
	CommitInfo commit_info;
};

struct ROB : Module<ROB_Input, ROB_Output> {
	bool isEmpty() const;
	bool isFull() const;
	int getIndex(max_size_t tag) const;
	// per-index field getters
	bool getValueReady(int i) const;
	bool getCommitReady(int i) const;
	max_size_t getValue(int i) const;
	max_size_t getTag(int i) const;
	max_size_t getPredPC(int i) const;
	max_size_t getPC(int i) const;
	max_size_t getDest(int i) const;
	ROBType getType(int i) const;
	bool getHalt(int i) const;
	max_size_t getCkpt(int i) const;
	// head getters (no-arg, wire-bindable)
	bool headCommitReady() const;
	bool headValueReady() const;
	max_size_t headValue() const;
	max_size_t headDest() const;
	bool headHalt() const;
	ROBType headType() const;
	max_size_t headPC() const;
	max_size_t headPredPC() const;
	max_size_t headTag() const;
	// write helpers (work() only)
	void push(const ROBEntry &entry);
	void pop();
	void flush(max_size_t flushTag);
	void work() override;
};

} // namespace dark
#endif // ROB_HPP
