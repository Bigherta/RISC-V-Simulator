#pragma once
#include <cstring>
#ifndef BRANCHPREDICTOR_HPP
#define BRANCHPREDICTOR_HPP
#include "common.hpp"
#include <cstdint>
struct BRU;
struct ROB;
struct systemState;

// BP update arbitration input: both table-training sources (BRU branch results
// and CDB JAL/JALR transfers) converge to this single point so that the two
// update calls keep a fixed order (BRU candidate first) regardless of stage
// scheduling order (see AGENTS.md "BP 更新（多写端）").
struct BPUpdateInput {
  const BRU &BRUModule;
  CDBOutput cdbArbiter;
  const ROB &ROBModule;
  SquashInfo squashDetect;
  BPUpdateInput(const BRU &bru, const ROB &rob)
      : BRUModule(bru), ROBModule(rob) {}
};

class BranchPredictor {
private:
  uint8_t globalPHT[PC_Direct_CAP] = {};
  uint8_t LHT[LHT_CAP] = {};
  uint8_t localPHT[LOCAL_PHT_CAP] = {};
  uint8_t selector[SELECTOR_CAP] = {};
  BTBEntry BTB[PC_Direct_CAP] = {};
  uint32_t RAS[RAS_CAP] = {};
  int RAS_top = 0;
  uint16_t GHR = 0;
public:
  BranchPredictor() {
    std::memset(globalPHT, 1, sizeof(globalPHT));
    std::memset(localPHT, 1, sizeof(localPHT));
    std::memset(selector, 1, sizeof(selector));
  }
  PredictInfo predict(int32_t pc);
  void update(int32_t pc, bool taken, int32_t target, uint16_t ghr);
  void updateJump(int32_t pc, int32_t target);
  void shiftGHR(bool taken);
  void RAS_push(uint32_t);
  auto RAS_pop() -> uint32_t;
  auto RAS_empty() const -> bool;
  auto RAS_full() const -> bool;
  BranchPredictorSnapshot snapshotCheckPoint() const;
  void recoverCheckPoint(const BranchPredictorSnapshot&);
  void tick(const BPUpdateInput &, systemState &);
};
#endif // BRANCHPREDICTOR_HPP