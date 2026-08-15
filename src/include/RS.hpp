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
  int robIndex = ~0u >> 1;
  ReservationStation() : free(true) {}
  ReservationStation(Operation type) : op(type), free(true) {}
};

struct StoreValueReservationStation {
  bool free;
  int32_t vrs2 = 0;
  int qrs2 = -1;
  int robIndex = ~0u >> 1;
  StoreValueReservationStation() : free(true) {}
};

struct BranchReservationStation : public ReservationStation {
  int32_t imm;
  uint32_t pc;
  BranchReservationStation() : ReservationStation() {}
  BranchReservationStation(Operation type) : ReservationStation(type) {}
};
struct ROB;
struct systemState;
struct RSInput {
  SquashInfo squashDetect;
  const ROB &ROBModule;        
  DispatchBus dispatchBus;              // 派发释放（read() 填充，快照边求值）
  RSInput(const ROB &rob) : ROBModule(rob) {}
};
class RSUnit {
  friend struct ReorderTester;
public:                           
  ReservationStation integerRS[INTEGERRS_CAP];
  ReservationStation loadRS[LOADRS_CAP];
  ReservationStation storeAddressRS[STORERS_CAP];
  StoreValueReservationStation storeValueRS[STORERS_CAP];
  BranchReservationStation branchRS[BRANCHRS_CAP];
  int tryAllocInteger() const;
  int tryAllocLoad() const;
  int tryAllocStoreAddress() const;
  int tryAllocStoreValue() const;
  int tryAllocBranch() const;
  void tick(const RSInput&, systemState&);  
};
#endif // RS_HPP
