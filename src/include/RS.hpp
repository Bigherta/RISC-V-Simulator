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

struct StoreMicroReservationStation {
  bool free;
  int32_t vrs2 = 0;
  int qrs2 = -1;
  int robIndex = ~0u >> 1;
  StoreMicroReservationStation() : free(true) {}
};

struct BranchReservationStation : public ReservationStation {
  int32_t imm;
  uint32_t pc;
  BranchReservationStation() : ReservationStation() {}
  BranchReservationStation(Operation type) : ReservationStation(type) {}
};

struct RSCluster {
  ReservationStation IntegerRS[INTEGERRS_CAP];
  ReservationStation StoreRS[STORERS_CAP];
  StoreMicroReservationStation MicroStoreRS[STORERS_CAP];
  ReservationStation LoadRS[LOADRS_CAP];
  BranchReservationStation BranchRS[BRANCHRS_CAP];
  bool isIntergerRSFull() const {
    for (auto IntegerRS : IntegerRS) {
      if (IntegerRS.free) {
        return false;
      }
    }
    return true;
  }
  bool isStoreRSFull() const {
    for (auto StoreRS : StoreRS) {
      if (StoreRS.free) {
        return false;
      }
    }
    return true;
  }
  bool isMicroStoreRSFull() const {
    for (auto MicroRS : MicroStoreRS) {
      if (MicroRS.free) {
        return false;
      }
    }
    return true;
  }
  bool isLoadRSFull() const {
    for (auto LoadRS : LoadRS) {
      if (LoadRS.free) {
        return false;
      }
    }
    return true;
  }
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