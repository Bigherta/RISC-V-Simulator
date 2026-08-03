#pragma once
#include <cstdint>
#ifndef BRU_HPP
#define BRU_HPP
#include "common.hpp"
class BRU {
private:
  BranchResult outputBuffer[4];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  bool isFull() const;
  bool isEmpty() const;
  void BRUExecute(int32_t op1, int32_t op2, int32_t pc, int32_t imm,
                  Operation op, int ROBTag);
  void push(BranchResult);
  void flush(int tag);
  BranchResult pop();
  BranchResult peek() const;
};
#endif // BRU_HPP
