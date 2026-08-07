#pragma once
#ifndef CDB_ARB_HPP
#define CDB_ARB_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// ALU side bound to combined getters (isEmpty/peekTag/peekValue/peekIsAddress/peekIsControl),
// LSQ side bound to cdbDetect (M4a: left unbound / dummy 0)
struct CDB_Arb_Input {
	Wire<1> alu_valid;
	Wire<31> alu_tag;
	Wire<32> alu_value;
	Wire<1> alu_is_address;
	Wire<1> alu_is_control;
	Wire<1> lsq_valid;
	Wire<31> lsq_tag;
	Wire<32> lsq_value;
	Wire<6> lsq_index;
	Wire<1> squash_valid;
	Wire<31> squash_tag;
};

// stateless combinational arbitration (ports CDBArbiter::arbitrate):
// sources with tag > squash tag are invalidated; smaller rob_tag wins
struct CDB_Arb : Module<CDB_Arb_Input, EmptyOutput> {
	bool resultValid() const;
	max_size_t resultTag() const;
	max_size_t resultValue() const;
	bool resultIsAddress() const;
	bool resultIsControl() const;
	bool aluGranted() const; // for ALU.remove
	bool lsqGranted() const; // for LSQ isCDBBroadcast
	max_size_t lsqIndex() const;
	void work() override; // empty
};

} // namespace dark
#endif // CDB_ARB_HPP
