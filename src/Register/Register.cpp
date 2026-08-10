#include "../include/Register.hpp"

RegCluster::RegCluster() { std::memset(RAT, 0xFF, sizeof(RAT)); }

int32_t RegCluster::readReg(int regNum) const {
  if (regNum == 0)
    return 0;
  return reg[regNum].read();
}

void RegCluster::writeReg(int regNum, int32_t value) {
  if (regNum == 0)
    return;
  reg[regNum].write(value);
}

int RegCluster::readRAT(int regNum) const { return RAT[regNum]; }

void RegCluster::setRAT(int regNum, int robIndex) { RAT[regNum] = robIndex; }

OperandInfo RegCluster::readOperand(int regNum) const {
  if (regNum == 0)
    return {true, 0, -1};
  int robIndex = RAT[regNum];
  if (robIndex == -1) {
    return {true, readReg(regNum), -1};
  }
  return {false, 0, robIndex};
}

void RegCluster::resetX0() { reg[0].write(0); }

RATSnapshot RegCluster::snapshotRAT() const {
  RATSnapshot snapshot;
  memcpy(snapshot.RAT_snapshot, RAT, sizeof(RAT));
  return snapshot;
}

void RegCluster::restoreRAT(RATSnapshot snapshot) {
  memcpy(RAT, snapshot.RAT_snapshot, sizeof(RAT));
}