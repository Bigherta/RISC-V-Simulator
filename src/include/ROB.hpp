#pragma once
#ifndef ROB_HPP
#define ROB_HPP
#include <cstdint>
enum ROBType {
  REGISTER,
  BRANCH,
  STORE,
};

enum ROBState {
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
  ROBEntry() : type(REGISTER), state(Waiting) {}
  ROBEntry(ROBType type_) : type(type_), state(Waiting) {}
};

class ROB {
private:
  ROBEntry ROBqueue[65];
  int head = 0;
  int tail = 0;
  inline static int ROB_Tag = 1;
public:
  bool isFull();
  int push(ROBEntry entry);
  ROBEntry peek();
  ROBEntry pop();
  int getIndex(int Tag);
  void writeROB(int32_t value, int index, ROBState state);
};
#endif // ROB_HPP