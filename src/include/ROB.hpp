#pragma once
#ifndef ROB_HPP
#define ROB_HPP
#include "common.hpp"
#include <cstdint>
enum class ROBType {
  REGISTER,
  BRANCH,
  STORE,
};

enum class ROBState {
  Waiting,
  Executing,
  Ready,
};

struct ROBEntry {
  ROBType type;
  ROBState state;
  int tag;
  int dest; // if type is REGISTER, record its destination
  int32_t value;
  ROBEntry() : type(ROBType::REGISTER), state(ROBState::Waiting) {}
  ROBEntry(ROBType type_) : type(type_), state(ROBState::Waiting) {}
};

class ROB {
  friend struct ReorderTester;
private:
  ROBEntry ROBqueue[ROB_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;
  inline static uint64_t ROB_Tag = 1;
public:
  bool isFull() const;
  int push(ROBEntry entry);
  ROBEntry peek() const;
  ROBEntry pop();
  int getIndex(int Tag) const;
  ROBEntry getEntry(int Tag) const;
  void writeROB(int32_t value, int index, ROBState state);
};
#endif // ROB_HPP