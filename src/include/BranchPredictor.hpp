#pragma once
#ifndef BRANCHPREDICTOR_HPP
#define BRANCHPREDICTOR_HPP
#include "common.hpp"
#include <cstdint>
class BranchPredictor {
private:
  uint8_t BHT[BHT_CAP] = {};
  BTBEntry BTB[BTB_CAP] = {};
  uint32_t RAS[RAS_CAP] = {};
  int RAS_top = 0;

public:
  PredictInfo predict(int32_t pc);
  void update(int32_t pc, bool taken, int32_t target);
  void RAS_push(uint32_t);
  auto RAS_pop() -> uint32_t;
  auto RAS_empty() const -> bool;
  auto RAS_full() const -> bool;
  RASCheckPoint snapshotCheckPoint() const;
  void recoverCheckPoint(RASCheckPoint);
};
#endif // BRANCHPREDICTOR_HPP