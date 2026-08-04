#include "../include/BranchPredictor.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
PredictInfo BranchPredictor::predict(int32_t pc) {
  auto BHT_index = (pc >> 2) & (BHT_CAP - 1);
  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  bool hit = BTB[BTB_index].valid && BTB[BTB_index].actualPC == pc;
  bool taken = hit && BHT[BHT_index] >= 2;
  int32_t predictPC = pc + 4;
  if (taken && hit) {
    predictPC = BTB[BTB_index].target;
  }
  return {taken, predictPC};
}
void BranchPredictor::update(int32_t pc, bool taken, int32_t target) {
  auto BHT_index = (pc >> 2) & (BHT_CAP - 1);
  BHT[BHT_index] =
      taken ? (BHT[BHT_index] < 3 ? BHT[BHT_index] + 1 : BHT[BHT_index])
            : (BHT[BHT_index] > 0 ? BHT[BHT_index] - 1 : BHT[BHT_index]);
  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  if (taken) {
    BTB[BTB_index].actualPC = pc;
    BTB[BTB_index].target = target;
    BTB[BTB_index].valid = true;
  }
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

RASCheckPoint BranchPredictor::snapshotCheckPoint() const {
  RASCheckPoint ckpt;
  memcpy(ckpt.RAS, RAS, sizeof(RAS));
  ckpt.top = RAS_top;
  return ckpt;
}

void BranchPredictor::recoverCheckPoint(RASCheckPoint ckpt) {
  memcpy(RAS, ckpt.RAS, sizeof(RAS));
  RAS_top = ckpt.top;
}