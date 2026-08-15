#include "../include/RAT.hpp"

int RAT::readRAT_PRF(int regNum) const { return RAT_PRF[regNum]; }

void RAT::setRAT_PRF(int regNum, int PRF_id) {
  RAT_PRF[regNum] = PRF_id;
}

OperandInfo RAT::readOperand(int regNum) const {
  if (regNum == 0)
    return {true, 0, -1};
  int phy = RAT_PRF[regNum];
  return {false, 0, phy};
}

RATSnapshot RAT::snapshotRAT_PRF() const {
  RATSnapshot snapshot;
  memcpy(snapshot.RAT_snapshot, RAT_PRF, sizeof(RAT_PRF));
  return snapshot;
}

void RAT::restoreRAT_PRF(const RATSnapshot &snapshot) {
  memcpy(RAT_PRF, snapshot.RAT_snapshot, sizeof(RAT_PRF));
}