#pragma once
#include <cstdint>
#include <cstring>
#include "common.hpp"
#ifndef REGISTER_HPP
#define REGISTER_HPP

class Register {
private:
  int32_t data;

public:
  Register() : data(0) {}
  void write(int32_t data_) { data = data_; }
  int32_t read() const { return data; }
};

struct OperandInfo {
  bool ready;
  int32_t value;
  int phyRegIndex;
};

class RegCluster {
private:
  Register reg[REGISTER_CAP];
  int RAT_PRF[REGISTER_CAP];
public:
  RegCluster();

  int32_t readReg(int regNum) const;
  void writeReg(int regNum, int32_t value);

  int readRAT_PRF(int regNum) const;
  void setRAT_PRF(int regNum, int PRF_id);
  RATSnapshot snapshotRAT_PRF() const;
  void restoreRAT_PRF(const RATSnapshot &snapshot);
  OperandInfo readOperand(int regNum) const;
  void resetX0();
};

#endif // REGISTER_HPP