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
  bool isFull();
  bool isEmpty();
  void ALUExecute(int32_t op1, int32_t op2, Op op, int ROBTag);
  void push(ExecuteResult);
  ExecuteResult pop();
};
#endif