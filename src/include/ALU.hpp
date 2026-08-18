#pragma once
#ifndef ALU_HPP
#define ALU_HPP
#include "common.hpp"
#include "RS.hpp"
struct systemState;
struct ALUInput {
  SquashInfo squashDetect;
  const RSUnit &RSModule;
  CDBOutput cdbOut;
  DispatchInfo dispatch;               
  ALUInput(const RSUnit &rs)
      : RSModule(rs) {}
};
class ALU {
  friend struct ReorderTester;
private:
  ArithmeticCalculateResult outputBuffer[ALU_CAP];
  bool slotValid[ALU_CAP] = {};
public:
  bool isFull() const;
  bool isEmpty() const;
  void push(int32_t op1, int32_t op2, Operation op, RobTag robTag,
                bool isControl);
  void remove(uint8_t robTag);
  void flush(uint8_t tag);
  int32_t headValue() const;
  uint8_t headRobTag() const;
  bool headIsControl() const;
  bool isValid(int index) const { return slotValid[index]; }
  void tick(const ALUInput&, systemState&);
};
#endif // ALU_HPP
