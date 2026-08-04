#pragma once
#include <cstdint>
#ifndef ALU_HPP
#define ALU_HPP
#include "common.hpp"
class ALU {
private:
  ExecuteResult outputBuffer[ALU_CAP];
  bool slotValid[ALU_CAP] = {};
public:
  bool isFull() const;
  bool isEmpty() const;
  static int32_t ALUCalculate(int32_t op1, int32_t op2, Operation op);
  void push(ExecuteResult);
  void remove(int robTag);
  void flush(int tag);
  ExecuteResult peek() const;
  ExecuteResult getEntry(int index) const;
  bool isValid(int index) const { return slotValid[index]; }
};
#endif // ALU_HPP