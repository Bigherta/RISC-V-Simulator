#pragma once
#ifndef ALU_HPP
#define ALU_HPP
#include "common.hpp"
#include "RS.hpp"
struct systemState;
struct PRF;
struct ALUInput {
  SquashInfo squashDetect;
  const RSUnit &RSModule;
  const PRF &PRFModule;
  CDBOutput cdbOut;
  DispatchInfo dispatch;               
  ALUInput(const RSUnit &rs, const PRF &prf)
      : RSModule(rs), PRFModule(prf) {}
};
class ALU {
  friend struct ReorderTester;
private:
  ArithmeticCalculateResult outputBuffer[ALU_CAP];
  bool slotValid[ALU_CAP] = {};
  void push(int32_t op1, int32_t op2, Operation op, RobTag robTag,
                bool isControl);
  void remove(uint8_t robTag);
  void flush(uint8_t tag);
public:
  bool isFull() const;
  bool isEmpty() const;
  int32_t headValue() const;
  uint8_t headRobTag() const;
  bool headIsControl() const;
  bool isValid(int index) const { return slotValid[index]; }
  void tick(const ALUInput&, systemState&);
};
#endif // ALU_HPP
