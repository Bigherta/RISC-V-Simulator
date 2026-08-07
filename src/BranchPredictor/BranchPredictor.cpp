#include "../include/BranchPredictor.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
PredictInfo BranchPredictor::predict(int32_t pc) {
  auto local_index = (pc >> 2) & (BHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ GHR) & (BHT_CAP - 1);
  auto selector_index = ((pc >> 2) ^ GHR) & (SELECTOR_CAP - 1);
  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  bool hit = BTB[BTB_index].valid && BTB[BTB_index].actualPC == pc;
  bool use_global = selector[selector_index] >= 2;
  bool taken = hit && (use_global ? globalPHT[global_index] >= 2
                                  : localPHT[local_index] >= 2);
  int32_t predictPC = pc + 4;
  if (taken && hit) {
    predictPC = BTB[BTB_index].target;
  }
  return {taken, predictPC};
}
void BranchPredictor::update(int32_t pc, bool taken, int32_t target,
                             uint16_t ghr) {
  auto local_index = (pc >> 2) & (BHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ ghr) & (BHT_CAP - 1);
  auto selector_index = ((pc >> 2) ^ ghr) & (SELECTOR_CAP - 1);

  bool pred_local = localPHT[local_index] >= 2;
  bool pred_global = globalPHT[global_index] >= 2;

  localPHT[local_index] =
      taken
          ? (localPHT[local_index] < 3 ? localPHT[local_index] + 1
                                       : localPHT[local_index])
          : (localPHT[local_index] > 0 ? localPHT[local_index] - 1
                                       : localPHT[local_index]);
  globalPHT[global_index] =
      taken ? (globalPHT[global_index] < 3 ? globalPHT[global_index] + 1
                                           : globalPHT[global_index])
            : (globalPHT[global_index] > 0 ? globalPHT[global_index] - 1
                                           : globalPHT[global_index]);

  bool local_correct = pred_local == taken;
  bool global_correct = pred_global == taken;
  if (global_correct && !local_correct)
    selector[selector_index] =
        selector[selector_index] < 3 ? selector[selector_index] + 1
                                     : selector[selector_index];
  else if (local_correct && !global_correct)
    selector[selector_index] =
        selector[selector_index] > 0 ? selector[selector_index] - 1
                                     : selector[selector_index];

  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  if (taken) {
    BTB[BTB_index].actualPC = pc;
    BTB[BTB_index].target = target;
    BTB[BTB_index].valid = true;
  }
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

BranchPredictorCkpt BranchPredictor::snapshotCheckPoint() const {
  return {RAS_top, GHR};
}

void BranchPredictor::recoverCheckPoint(BranchPredictorCkpt ckpt) {
  RAS_top = ckpt.top;
  GHR = ckpt.GHR;
}