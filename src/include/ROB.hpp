#pragma once
#ifndef ROB_HPP
#define ROB_HPP
#include "common.hpp"
#include <cstring>
#include <cstdint>
enum class ROBType {
  REGISTER,
  BRANCH,
  STORE,
  LINK,
};
struct ROBEntry {
  ROBType type = ROBType::REGISTER;
  bool isCommitReady = false;
  bool isValueValid = false;
  uint64_t seq;
  int dest = 0; // if type is REGISTER, record its destination
  int32_t value = 0;
  uint32_t predictedPC = 0;
  int32_t pc = 0;
  bool halt = false;
  uint8_t lsqTailSnapshot = 0;
  int rat_ckpt[REGISTER_CAP];
  BranchPredictorSnapshot ras_ckpt{};
  ROBEntry() { std::memset(rat_ckpt, 0xFF, sizeof(rat_ckpt)); }
  ROBEntry(ROBType type_) : type(type_) {
    std::memset(rat_ckpt, 0xFF, sizeof(rat_ckpt));
  }
};

class ROB {
  friend struct ReorderTester;
  friend struct Reorder720Tester;

private:
  ROBEntry ROBqueue[ROB_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint64_t next_seq = 1;

public:
  bool isFull() const;
  bool isEmpty() const;
  bool isHeadCommitReady() const;
  bool isHeadValueValid() const;
  uint64_t headSeq() const;
  int push(ROBEntry entry);
  ROBEntry peek() const;
  ROBEntry pop();
  int getTail() const;
  int getHead() const;
  uint64_t getSeq(int index) const;
  bool isValueValidAt(int index) const;
  bool isCommitReadyAt(int index) const;
  int32_t getValue(int index) const;
  ROBType getType(int index) const;
  int getDest(int index) const;
  int32_t getPC(int index) const;
  bool getHalt(int index) const;
  const BranchPredictorSnapshot &getRASCkpt(int index) const;
  const int *getRATCkpt(int index) const;
  int getPredictedPC(int index) const;
  uint8_t getLsqTailSnapshot(int index) const;
  void writeROBValue(int32_t value, int index);
  void writeROBPredictedPC(uint32_t pc, int index);
  void setROBCommitReady(int index);
  void setROBValueValid(int index);
  void flush(int squashIndex);
};
#endif // ROB_HPP