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
  void push(int32_t op1, int32_t op2, Operation op, int robIndex, uint64_t robSeq,
            bool isAddress, bool isControl);
  void remove(uint64_t robSeq);
  void flush(uint64_t seq);
  ExecuteResult peek() const;
  ExecuteResult getEntry(int index) const;
  bool isValid(int index) const { return slotValid[index]; }
};
#endif // ALU_HPP