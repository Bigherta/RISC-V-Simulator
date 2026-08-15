#pragma once
#include "BRU.hpp"
#include "LSQ.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>
enum class ROBType {
  REGISTER,
  BRANCH,
  STORE,
  LINK,
};
struct ROBEntry {
  ROBType type = ROBType::REGISTER;
  bool isCommitReady = false;
  uint64_t seq;
  int dest = 0; // if type is REGISTER, record its destination
  uint32_t predictedPC = 0;
  int32_t pc = 0;
  bool halt = false;
  uint8_t lsqTailSnapshot = 0;
  Checkpoint ckpt{};
  int newPhy = -1;
  int oldPhy = -1;
  ROBEntry() {
    std::memset(ckpt.RATsnapshot.RAT_snapshot, 0xFF,
                sizeof(ckpt.RATsnapshot.RAT_snapshot));
  }
  ROBEntry(ROBType type_) : type(type_) {
    std::memset(ckpt.RATsnapshot.RAT_snapshot, 0xFF,
                sizeof(ckpt.RATsnapshot.RAT_snapshot));
  }
};
struct ROBInput {
  SquashInfo squashDetect;
  CDBOutput cdbArbiter;
  const BRU &BRUModule;
  const LSQ &LSQModule;
  ROBInput(const BRU &bru, const LSQ &lsq) : BRUModule(bru), LSQModule(lsq) {}
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
  uint64_t headSeq() const;
  int push(ROBEntry entry);
  ROBEntry peek() const;
  void pop();
  int getTail() const;
  int getHead() const;
  uint64_t getSeq(int index) const;
  bool isCommitReadyAt(int index) const;
  ROBType getType(int index) const;
  int getDest(int index) const;
  int32_t getPC(int index) const;
  bool getHalt(int index) const;
  const BranchPredictorSnapshot &getRASCkpt(int index) const;
  const int *getRATPrfCkpt(int index) const;
  int getPredictedPC(int index) const;
  uint8_t getLsqTailSnapshot(int index) const;
  uint32_t getFlHeadSeqCkpt(int index) const;
  int getNewPhy(int index) const;
  int getOldPhy(int index) const;
  void setROBCommitReady(int index);
  void flush(int squashIndex);
  void tick(const ROBInput &, systemState &);
};