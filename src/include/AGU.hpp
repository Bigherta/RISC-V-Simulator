#pragma once
#ifndef AGU_HPP
#define AGU_HPP
#include "common.hpp"
#include "RS.hpp"
#include "LSQ.hpp"
struct systemState;
struct AGUInput {
  SquashInfo squashDetect;
  const LSQ &LSQModule;
  const RSUnit &RSModule;
  DispatchInfo dispatch;              
  AGUInput(const LSQ &lsq, const RSUnit &rs)
      : LSQModule(lsq), RSModule(rs) {}
};
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
  void tick(const AGUInput&, systemState&);
};
#endif // AGU_HPP