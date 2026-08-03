#pragma once
#include <cstdint>
#ifndef ALU_HPP
#define ALU_HPP
#include "common.hpp"
class ALU {
private:
  ExecuteResult outputBuffer[4];
  uint8_t head = 0;
  uint8_t tail = 0;
public:
  bool isFull() const;
  bool isEmpty() const;
  static int32_t ALUCalculate(int32_t op1, int32_t op2, Operation op);
  void push(ExecuteResult);
  void flush(int tag);
  ExecuteResult pop();
  ExecuteResult peek() const;
};
#endif // ALU_HPP