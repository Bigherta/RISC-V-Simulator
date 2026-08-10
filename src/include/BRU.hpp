#pragma once
#include <cstdint>
#ifndef BRU_HPP
#define BRU_HPP
#include "common.hpp"
class BRU {
private:
  BranchResult outputBuffer[BRU_CAP];
  bool slotValid[BRU_CAP] = {};

public:
  bool isFull() const;
  bool isEmpty() const;
  void BRUExecute(int32_t op1, int32_t op2, int32_t pc, int32_t imm,
                  Operation op, int robIndex, uint64_t robSeq);
  void push(BranchResult);
  void remove(uint64_t robSeq);
  void flush(uint64_t seq);
  int32_t headPCFrom() const;
  int32_t headPCResult() const;
  int headRobIndex() const;
  uint64_t headRobSeq() const;
  bool isValid(int index) const { return slotValid[index]; }
};
#endif // BRU_HPP