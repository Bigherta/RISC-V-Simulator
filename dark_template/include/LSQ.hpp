#pragma once
#ifndef LSQ_HPP
#define LSQ_HPP
#include "common.hpp"
#include "helper/tools.h"

namespace dark {

// ValueState (ports src/include/LSQ.hpp:7-11)
enum class LSQValueState : max_size_t {
	NOTREADY,
	FETCHING,
	READY,
};

struct LSQWrite {
	int index;
	max_size_t value;
	max_size_t knownTag;
	bool setValue;
};

struct LSQPlan {
	std::array<LSQWrite, LSQ_CAP> writes;
	int count = 0;
};

struct LSQPush { // bound to RAT lsqPush* (same-cycle as ROB/RS push)
	Wire<1> valid;
	Wire<1> is_load;
	Wire<3> n_bytes;
	Wire<1> is_unsigned;
	Wire<31> rob_tag;
};

struct LSQMemReply { // M5: memory return; M4b-3: test flag
	Wire<1> valid;
	Wire<32> value;
	Wire<31> rob_tag;
};

struct LSQOperandChannel { // one STORE_RS operand slot (wired per-slot)
	Wire<1> valid;
	Wire<32> value;
	Wire<31> tag;
};

struct LSQRobState { // wired via lambda (CPU.cpp:696-698 / 933)
	Wire<1> store_dispatch_ok; // getIndex(headStoreTag)==-1 || (==rob head && commit-ready)
	Wire<1> head_load_absent; // getIndex(headLoadTag) == -1
};

struct LSQ_Input {
	LSQPush push;
	CDBIn cdb; // address write: valid && is_address
	std::array<LSQOperandChannel, STORERS_CAP> operand;
	LSQMemReply mem_reply;
	LSQRobState rob;
	Wire<1> cdb_lsq_granted; // CDB_Arb.lsqGranted()
	Wire<6> cdb_lsq_index; // CDB_Arb.lsqIndex()
	Wire<1> mem_busy; // MEM.isBusyOrReq() (in-flight request blocks dispatch)
	SquashReq squash;
};

struct LSQMemRequest { // written by work, read by Memory (M5)
	Register<1> valid;
	Register<1> is_load;
	Register<32> address;
	Register<32> value;
	Register<3> n_bytes;
	Register<1> is_signed; // !is_unsigned
	Register<31> rob_tag;
};

struct LSQ_Output {
	std::array<Register<32>, LSQ_CAP> slot_address;
	std::array<Register<32>, LSQ_CAP> slot_value;
	std::array<Register<3>, LSQ_CAP> slot_n_bytes;
	std::array<Register<31>, LSQ_CAP> slot_known_tag;
	std::array<Register<31>, LSQ_CAP> slot_rob_tag;
	std::array<Register<1>, LSQ_CAP> slot_is_unsigned;
	std::array<Register<1>, LSQ_CAP> slot_is_load;
	std::array<Register<1>, LSQ_CAP> slot_is_address_ready;
	std::array<Register<2>, LSQ_CAP> slot_value_state; // LSQValueState
	std::array<Register<1>, LSQ_CAP> slot_is_cdb_broadcast;
	Register<6> head;
	Register<6> tail;
	LSQMemRequest mem_request; // packed group
};

struct LSQ : Module<LSQ_Input, LSQ_Output> {
	bool isEmpty() const;
	bool isFull() const;
	int getIndex(max_size_t tag) const;
	// per-index getters
	bool isLoad(int i) const;
	bool isAddressReady(int i) const;
	max_size_t getValue(int i) const;
	max_size_t getTag(int i) const;
	max_size_t getAddress(int i) const;
	max_size_t getNBytes(int i) const;
	bool getIsUnsigned(int i) const;
	max_size_t getValueState(int i) const;
	bool getIsCDBBroadcast(int i) const;
	max_size_t getKnownTag(int i) const;
	bool isReadyToCommit(int i) const; // CPU.cpp:735 / LSQ.cpp:219
	// head getters
	bool headStoreValid() const;
	max_size_t headStoreTag() const;
	bool headLoadValid() const;
	max_size_t headLoadTag() const;
	// combined detectors (ports LSQ.cpp)
	int loadDetect() const; // 0xFFFFFFFF = none
	int cdbDetectIndex() const; // 0xFFFFFFFF = none (loads only, LSQ.cpp:204)
	int lsqReadyIndex() const; // load or store ready & unbroadcast (CDB_Arb lsq side)
	LSQPlan planDataForward(int index, max_size_t value) const; // LSQ.cpp:83
	LSQPlan planAddressForward(int index, max_size_t address) const; // LSQ.cpp:111
	void work() override;
};

} // namespace dark
#endif // LSQ_HPP
