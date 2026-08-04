#include "../include/BranchPredictor.hpp"
#include <cstdint>
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