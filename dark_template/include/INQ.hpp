#pragma once
#ifndef INQ_HPP
#define INQ_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct INQ_PushReq { // fetch request (M3 stub / M6 bound to IMEM)
	Wire<1> valid;
	Wire<32> raw;
	Wire<32> pc;
	Wire<32> predpc;
	Wire<32> ckpt; // packed top|GHR
};
struct INQ_SquashReq {
	Wire<1> valid;
};

struct INQ_Input {
	INQ_PushReq push_req;
	INQ_SquashReq squash;
	Wire<1> pop_valid; // bound to Decoder.consumingValid() (same-cycle pop)
};

struct INQ_Output { // fetch-stage data only (decoded fields live in Decoder)
	std::array<Register<32>, INQ_CAP> slot_raw;
	std::array<Register<32>, INQ_CAP> slot_pc;
	std::array<Register<32>, INQ_CAP> slot_predpc;
	std::array<Register<1>, INQ_CAP> slot_isHalt;
	std::array<Register<32>, INQ_CAP> slot_ckpt;
};

struct INQ_Private {
	Register<8> head;
	Register<8> tail;
};

struct INQ : Module<INQ_Input, INQ_Output, INQ_Private> {
	// read helpers
	bool isEmpty() const;
	bool isFull() const;
	// head getters (no-arg, wire-bindable)
	bool headValid() const;
	bool headIsHalt() const;
	max_size_t headRaw() const;
	max_size_t headPC() const;
	max_size_t headPredPC() const;
	max_size_t headCkpt() const;
	// write helpers (work() only)
	void push(max_size_t raw, max_size_t pc, max_size_t predpc, max_size_t ckpt);
	void pop();
	void clear();
	void work() override;
};

} // namespace dark
#endif // INQ_HPP
