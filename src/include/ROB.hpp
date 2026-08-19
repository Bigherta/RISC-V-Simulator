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
  uint8_t tag;
  int dest = 0; // if type is REGISTER, record its destination
  uint32_t predictedPC = 0;
  int32_t pc = 0;
  bool halt = false;
  bool isCall = false; // JAL rd==1
  bool isRet = false;  // JALR x0, 0(x1)
  uint8_t lsqTailSnapshot = 0;
  uint8_t ckptId = 0;
  int newPhy = -1;
  int oldPhy = -1;
};
struct IssuePacket;
struct ROBInput {
  SquashInfo squashDetect;
  CDBOutput cdbOut;
  const BRU &BRUModule;
  const LSQ &LSQModule;
  const IssuePacket &issuePacket;
  ROBInput(const BRU &bru, const LSQ &lsq, const IssuePacket &pkt)
      : BRUModule(bru), LSQModule(lsq), issuePacket(pkt) {}
};
class ROB {
  friend struct ReorderTester;

private:
  ROBEntry ROBqueue[ROB_CAP];
  uint8_t head = 0;
  uint8_t next_tag = 0;
  bool haltCommitted = false;
  int haltRd = -1;

public:
  using RobTag = ::RobTag;
  static bool isOlder(RobTag tag_a, RobTag tag_b);
  static bool isYounger(RobTag tag_a, RobTag tag_b);
  static int idx(RobTag t);
  int getIndexByTag(RobTag tag) const;
  bool isFull() const;
  bool isEmpty() const;
  bool isHaltCommitted() const;
  int getHaltRd() const;
  bool isHeadCommitReady() const;
  bool isHeadHalt() const;
  ROBType headType() const;
  int headDest() const;
  uint8_t getNextTag() const;
  void updateNextTag();
  int push(ROBEntry entry);
  void pop();
  int getHead() const;
  uint8_t getTag(int index) const;
  bool isCommitReadyAt(int index) const;
  ROBType getType(int index) const;
  int getDest(int index) const;
  int32_t getPC(int index) const;
  bool isHalt(int index) const;
  uint8_t getCkptId(int index) const;
  int getPredictedPC(int index) const;
  uint8_t getLsqTailSnapshot(int index) const;
  int getNewPhy(int index) const;
  int getOldPhy(int index) const;
  bool isCall(int index) const;
  bool isRet(int index) const;
  void setROBCommitReady(int index);
  void flush(int squashIndex);
  void tick(const ROBInput &, systemState &);
};