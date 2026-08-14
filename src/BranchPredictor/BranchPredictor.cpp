#include "../include/BranchPredictor.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

void BranchPredictor::tick(const BPUpdateInput &input, systemState &CPUstate) {
  struct Cand {
    bool valid = false;
    uint64_t seq = 0;
    int32_t pc = 0;
    bool taken = false;
    int32_t target = 0;
    uint16_t ghr = 0;
    bool cond = true;
  } bru, cdb;

  // collect candidate 1: BRU (B-type branch completed)
  if (!input.BRUModule.isEmpty()) {
    int index = input.BRUModule.headRobIndex();
    uint64_t brRobSeq = input.BRUModule.headRobSeq();
    int pcResult = input.BRUModule.headPCResult();
    int pcFrom = input.BRUModule.headPCFrom();
    if (index >= 0) {
      ++CPUstate.branchTotal;
      bool correct = pcResult == input.ROBModule.getPredictedPC(index);
      if (correct)
        ++CPUstate.branchCorrect;
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           brRobSeq < input.squashDetect.SquashSeq)) {
        bru.valid = true;
        bru.seq = brRobSeq;
        bru.pc = pcFrom;
        bru.taken = pcResult != pcFrom + 4;
        bru.target = pcResult;
        bru.ghr = input.ROBModule.getRASCkpt(index).GHR_snapshot;
      }
    }
  }
  // collect candidate 2: CDB (JAL/JALR control transfer completed)
  auto cdbOut = input.cdbArbiter;
  if (cdbOut.valid && cdbOut.result.isControl && !input.ROBModule.isEmpty() &&
      cdbOut.result.robSeq >= input.ROBModule.headSeq()) {
    auto robIndex = cdbOut.result.robIndex;
    auto robSeq = cdbOut.result.robSeq;
    const auto pc = static_cast<uint32_t>(cdbOut.result.value);
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         robSeq < input.squashDetect.SquashSeq)) {
      ++CPUstate.branchTotal;
      bool correct = pc == input.ROBModule.getPredictedPC(robIndex);
      if (correct)
        ++CPUstate.branchCorrect;
      cdb.valid = true;
      cdb.seq = robSeq;
      cdb.pc = input.ROBModule.getPC(robIndex);
      cdb.taken = true;
      cdb.target = static_cast<int32_t>(pc);
      cdb.ghr = input.ROBModule.getRASCkpt(robIndex).GHR_snapshot;
      cdb.cond = false;
    }
  }

  // direction-split: conditional branches train BHT/LHT/Gshare + BTB,
  // JAL/JALR train only the BTB (with unconditional flag), so the direction
  // tables are never polluted by always-taken jumps.
  auto apply = [&](const Cand &c) {
    if (c.cond)
      CPUstate.BPModule.update(c.pc, c.taken, c.target, c.ghr);
    else
      CPUstate.BPModule.updateJump(c.pc, c.target);
  };
  // fixed order: BRU candidate first, CDB candidate second
  if (bru.valid)
    apply(bru);
  if (cdb.valid)
    apply(cdb);
}
PredictInfo BranchPredictor::predict(int32_t pc) {
  auto local_index = (pc >> 2) & (LHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ GHR) & (PC_Direct_CAP - 1);
  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  auto selector_index = ((pc >> 2) ^ GHR) & (SELECTOR_CAP - 1);
  auto history = LHT[local_index];
  bool hit = BTB[BTB_index].valid && BTB[BTB_index].actualPC == pc;
  bool use_global = selector[selector_index] >= 2;
  bool taken;
  if (hit && BTB[BTB_index].unconditional)
    taken = true;
  else
    taken = hit && (use_global ? globalPHT[global_index] >= 2
                               : localPHT[history] >= 2);
  int32_t predictPC = pc + 4;
  if (taken && hit) {
    predictPC = BTB[BTB_index].target;
  }
  return {taken, predictPC};
}
void BranchPredictor::update(int32_t pc, bool taken, int32_t target,
                             uint16_t ghr) {
  auto local_index = (pc >> 2) & (LHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ ghr) & (PC_Direct_CAP - 1);
  auto selector_index = ((pc >> 2) ^ ghr) & (SELECTOR_CAP - 1);
  auto history = LHT[local_index];

  bool pred_local = localPHT[history] >= 2;
  bool pred_global = globalPHT[global_index] >= 2;

  localPHT[history] =
      taken
          ? (localPHT[history] < 3 ? localPHT[history] + 1 : localPHT[history])
          : (localPHT[history] > 0 ? localPHT[history] - 1 : localPHT[history]);
  LHT[local_index] =
      ((LHT[local_index] << 1) | (taken ? 1 : 0)) & (LOCAL_PHT_CAP - 1);
  globalPHT[global_index] =
      taken ? (globalPHT[global_index] < 3 ? globalPHT[global_index] + 1
                                           : globalPHT[global_index])
            : (globalPHT[global_index] > 0 ? globalPHT[global_index] - 1
                                           : globalPHT[global_index]);

  bool local_correct = pred_local == taken;
  bool global_correct = pred_global == taken;
  if (global_correct && !local_correct)
    selector[selector_index] = selector[selector_index] < 3
                                   ? selector[selector_index] + 1
                                   : selector[selector_index];
  else if (local_correct && !global_correct)
    selector[selector_index] = selector[selector_index] > 0
                                   ? selector[selector_index] - 1
                                   : selector[selector_index];

  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  if (taken) {
    BTB[BTB_index].actualPC = pc;
    BTB[BTB_index].target = target;
    BTB[BTB_index].valid = true;
    BTB[BTB_index].unconditional = false;
  }
}
void BranchPredictor::updateJump(int32_t pc, int32_t target) {
  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  BTB[BTB_index].actualPC = pc;
  BTB[BTB_index].target = target;
  BTB[BTB_index].valid = true;
  BTB[BTB_index].unconditional = true;
}
void BranchPredictor::shiftGHR(bool taken) {
  GHR = ((GHR << 1) | (taken ? 1 : 0)) & HISTORY_MASK;
}
void BranchPredictor::RAS_push(uint32_t addr) {
  if (RAS_top == RAS_CAP)
    throw std::runtime_error("RAS overflow: push in full return-address stack");
  RAS[RAS_top++] = addr;
}

uint32_t BranchPredictor::RAS_pop() {
  if (RAS_top == 0)
    throw std::runtime_error(
        "RAS underflow: pop on empty return-address stack");
  return RAS[--RAS_top];
}

bool BranchPredictor::RAS_empty() const { return RAS_top == 0; }
bool BranchPredictor::RAS_full() const { return RAS_top == RAS_CAP; }

BranchPredictorSnapshot BranchPredictor::snapshotCheckPoint() const {
  BranchPredictorSnapshot s;
  s.top_snapshot = RAS_top;
  s.GHR_snapshot = GHR;
  memcpy(s.RAS_snapshot, RAS, sizeof(RAS));
  return s;
}

void BranchPredictor::recoverCheckPoint(const BranchPredictorSnapshot &ckpt) {
  RAS_top = ckpt.top_snapshot;
  GHR = ckpt.GHR_snapshot;
  memcpy(RAS, ckpt.RAS_snapshot, sizeof(RAS));
}