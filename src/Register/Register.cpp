#include "../include/Register.hpp"

RegCluster::RegCluster() {
  std::memset(RegisterTable, 0xFF, sizeof(RegisterTable));
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

int RegCluster::readRAT(int regNum) const { return RegisterTable[regNum]; }

void RegCluster::setRAT(int regNum, int robTag) {
  RegisterTable[regNum] = robTag;
}

OperandInfo RegCluster::readOperand(int regNum) const {
  int tag = RegisterTable[regNum];
  if (tag == -1) {
    return {true, readReg(regNum), -1};
  }
  return {false, 0, tag};
}

void RegCluster::resetX0() { reg[0].write(0); }