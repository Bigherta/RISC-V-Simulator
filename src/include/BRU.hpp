#pragma once
#include <cstdint>
#include "common.hpp"
#include "RS.hpp"
struct systemState;
struct BRUInput {
  SquashInfo squashDetect;
  const ROB &ROBModule;
  const RSUnit &RSModule;
  DispatchInfo dispatch;                
  BRUInput(const ROB &rob, const RSUnit &rs)
      : ROBModule(rob), RSModule(rs) {}
};
class BRU {
  friend struct ReorderTester;
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
  void tick(const BRUInput&, systemState&);
};