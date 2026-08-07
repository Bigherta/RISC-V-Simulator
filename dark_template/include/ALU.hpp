#pragma once
#ifndef ALU_HPP
#define ALU_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

struct ALU_Input {
	Wire<1> rs_valid; // execute candidate ready
	Wire<6> rs_op; // Operation
	Wire<32> rs_vj;
	Wire<32> rs_vk;
	Wire<31> rs_tag; // robTag
	Wire<1> squash_valid; // flush request
	Wire<31> squash_tag;
	Wire<1> cdb_valid; // cdb broadcast (remove)
	Wire<31> cdb_tag;
};

struct ALU_Output {
	std::array<Register<1>, ALU_CAP> valid;
	std::array<Register<32>, ALU_CAP> value;
	std::array<Register<31>, ALU_CAP> tag;
	std::array<Register<1>, ALU_CAP> isAddress;
	std::array<Register<1>, ALU_CAP> isControl;
	Register<31> result_tag; // robTag of result
	Register<32> result_value;
	Register<1> result_valid;
	Register<1> result_isAddress; // load/store address calc
	Register<1> result_isControl; // JALR
};

struct ALU : Module<ALU_Input, ALU_Output> {
	int32_t ALUCalculate(int32_t op1, int32_t op2, Operation op);
	bool isEmpty() const;
	bool isFull() const;
	max_size_t peekTag() const;
	max_size_t peekValue() const;
	bool peekIsAddress() const;
	bool peekIsControl() const;
	bool push(ExecuteResult result); // false if buffer full
	void remove(max_size_t robTag);
	void flush(max_size_t flushTag);
	void work() override;
};

} // namespace dark
#endif // ALU_HPP
