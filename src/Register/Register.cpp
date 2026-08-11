#include "../include/Register.hpp"

RegCluster::RegCluster() {
  std::memset(RAT_PRF, 0xFF, sizeof(RAT_PRF));
  for (int i = 1; i < 32; i++)
    RAT_PRF[i] = i;
}

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

int RegCluster::readRAT_PRF(int regNum) const { return RAT_PRF[regNum]; }

void RegCluster::setRAT_PRF(int regNum, int PRF_id) {
  RAT_PRF[regNum] = PRF_id;
}

OperandInfo RegCluster::readOperand(int regNum) const {
  if (regNum == 0)
    return {true, 0, -1};
  int phy = RAT_PRF[regNum];
  if (phy == -1)
    return {true, readReg(regNum), -1};
  return {false, 0, phy};
}

void RegCluster::resetX0() { reg[0].write(0); }

RATSnapshot RegCluster::snapshotRAT_PRF() const {
  RATSnapshot snapshot;
  memcpy(snapshot.RAT_snapshot, RAT_PRF, sizeof(RAT_PRF));
  return snapshot;
}

void RegCluster::restoreRAT_PRF(const RATSnapshot &snapshot) {
  memcpy(RAT_PRF, snapshot.RAT_snapshot, sizeof(RAT_PRF));
}