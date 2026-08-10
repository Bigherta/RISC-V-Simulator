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

struct IntegerRS {
  ReservationStation IntegerRS[INTEGERRS_CAP];
  bool isIntegerRSFull() const {
    for (auto IntegerRS : IntegerRS) {
      if (IntegerRS.free) {
        return false;
      }
    }
    return true;
  }
};

struct StoreAddressRS {
  ReservationStation StoreAddressRS[STORERS_CAP];
  bool isStoreAddressRSFull() const {
    for (auto StoreAddressRS : StoreAddressRS) {
      if (StoreAddressRS.free) {
        return false;
      }
    }
    return true;
  }
};

struct StoreValueRS {
  StoreValueReservationStation StoreValueRS[STORERS_CAP];
  bool isStoreValueRSFull() const {
    for (auto StoreValueRS : StoreValueRS) {
      if (StoreValueRS.free) {
        return false;
      }
    }
    return true;
  }
};

struct LoadRS {
  ReservationStation LoadRS[LOADRS_CAP];
  bool isLoadRSFull() const {
    for (auto LoadRS : LoadRS) {
      if (LoadRS.free) {
        return false;
      }
    }
    return true;
  }
};

struct BranchRS {
  BranchReservationStation BranchRS[BRANCHRS_CAP];
  bool isBranchRSFull() const {
    for (auto BranchRS : BranchRS) {
      if (BranchRS.free) {
        return false;
      }
    }
    return true;
  }
};
#endif // RS_HPP