#pragma once
#ifndef ROB_HPP
#define ROB_HPP
#include "common.hpp"
#include <cstdint>
enum class ROBType {
  REGISTER,
  BRANCH,
  STORE,
  LINK,
};

enum class ROBState {
  Waiting,
  ValueReady,
  CommitReady,
};

struct ROBEntry {
  ROBType type;
  ROBState state;
  int tag;
  int dest; // if type is REGISTER, record its destination
  int32_t value = 0;
  uint32_t predictedPC = 0;
  int32_t pc = 0;
  bool halt = false;
  BranchPredictorCkpt ras_ckpt;
  ROBEntry() : type(ROBType::REGISTER), state(ROBState::Waiting) {}
  ROBEntry(ROBType type_) : type(type_), state(ROBState::Waiting) {}
};

class ROB {
  friend struct ReorderTester;
  friend struct Reorder720Tester;

private:
  ROBEntry ROBqueue[ROB_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint64_t ROB_Tag = 1;

public:
  bool isFull() const;
  bool isEmpty() const;
  int ClearRATDest() const;
  int headROB() const;
  ROBState headState() const;
  int push(ROBEntry entry);
  ROBEntry peek() const;
  ROBEntry pop();
  int getTail() const;
  int getHead() const;
  int getIndex(int Tag) const;
  ROBEntry getEntry(int index) const;
  int getPredictedPC(int index) const;
  void writeROBValue(int32_t value, int index);
  void writeROBPredictedPC(uint32_t pc, int index);
  void writeROBState(ROBState state, int index);
  uint64_t currentTag() const;
  void flush(int tag);
};
#endif // ROB_HPP