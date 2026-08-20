#pragma once
#ifndef RS_HPP
#define RS_HPP
#include "common.hpp"
#include <cstdint>
struct ReservationStation {
  Operation op;
  bool free;
  int32_t vj = 0;
  int32_t vk = 0;
  int qj = -1;
  int qk = -1;
  RobTag robTag = 0xFF;
  ReservationStation() : free(true) {}
  ReservationStation(Operation type) : op(type), free(true) {}
};

struct AddressRS : public ReservationStation {
  uint8_t memIndex = 0;
  AddressRS() : ReservationStation() {}
  AddressRS(Operation type) : ReservationStation(type) {}
};

struct LoadAddressRS : public AddressRS {
  LoadAddressRS() : AddressRS() {}
};

struct StoreAddressRS : public AddressRS {
  StoreAddressRS() : AddressRS() {}
};

struct StoreValueReservationStation {
  bool free;
  int32_t vrs2 = 0;
  int qrs2 = -1;
  RobTag robTag = 0xFF;
  uint8_t memIndex = 0;
  StoreValueReservationStation() : free(true) {}
};

struct BranchReservationStation : public ReservationStation {
  int32_t imm;
  uint32_t pc;
  BranchReservationStation() : ReservationStation() {}
  BranchReservationStation(Operation type) : ReservationStation(type) {}
};
struct ROB;
struct IssuePacket;
struct systemState;
struct RSInput {
  SquashInfo squashDetect;
  const ROB &ROBModule;        
  DispatchBus dispatchBus;
  CDBBus cdbBus;
  const IssuePacket &issuePacket;
  RSInput(const ROB &rob, const IssuePacket &pkt)
      : ROBModule(rob), issuePacket(pkt) {}
};
class RSUnit {
  friend struct ReorderTester;
public:                           
  ReservationStation integerRS[INTEGERRS_CAP];
  LoadAddressRS loadRS[LOADRS_CAP];
  StoreAddressRS storeAddressRS[STORERS_CAP];
  StoreValueReservationStation storeValueRS[STORERS_CAP];
  BranchReservationStation branchRS[BRANCHRS_CAP];
  int tryAllocInteger() const;
  int tryAllocLoad() const;
  int tryAllocStoreAddress() const;
  int tryAllocStoreValue() const;
  int tryAllocBranch() const;
  void broadcast(const RSInput&, systemState&, RobTag robTag, int value);
  void tick(const RSInput&, systemState&);  
};
#endif // RS_HPP
