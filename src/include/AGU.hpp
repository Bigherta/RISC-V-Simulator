#pragma once
#ifndef AGU_HPP
#define AGU_HPP
#include "common.hpp"
class AGU {
  friend struct ReorderTester;
private:
  AddressCalculateResult outputBuffer[AGU_CAP];
  bool slotValid[AGU_CAP] = {};
public:
  bool isFull() const;
  bool isEmpty() const;
  void push(int32_t op1, int32_t op2, Operation op, int robIndex, uint64_t robSeq);
  void remove(uint64_t robSeq);
  void flush(uint64_t seq);
  int32_t headValue() const;
  int headRobIndex() const;
  uint64_t headRobSeq() const;
  bool isValid(int index) const { return slotValid[index]; }
};
#endif // AGU_HPP