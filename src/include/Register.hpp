#pragma once
#include <cstdint>
#include <cstring>
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
  int robTag;
};

class RegCluster {
private:
  Register reg[32];
  int RegisterTable[32];

public:
  RegCluster();

  int32_t readReg(int regNum) const;
  void writeReg(int regNum, int32_t value);

  int readRAT(int regNum) const;
  void setRAT(int regNum, int robTag);

  OperandInfo readOperand(int regNum) const;

  void resetX0();
};

#endif // REGISTER_HPP