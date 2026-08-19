#pragma once
#include "LSQ.hpp"
#include "ROB.hpp"
#include "common.hpp"
#include <cstdint>
struct IssuePacket;
struct PRFEntry {
  int32_t value;
  bool ready;
};
struct PRFInput {
  const IssuePacket &issuePacket;
  SquashInfo squashDetect;
  CDBOutput cdbOut;
  const LSQ &LSQModule;
  const ROB &ROBModule;
  PRFInput(const LSQ &lsq, const ROB &rob, const IssuePacket &pkt)
      : LSQModule(lsq), ROBModule(rob), issuePacket(pkt) {}
};
struct systemState;
class PRF {
  friend struct ReorderTester;

private:
  PRFEntry PhysicalRegs[PRF_CAP];
  uint8_t freeList[PRF_CAP];
  uint32_t PRFHeadCkpt[CKPT_CAP];
  uint32_t headSeq = 0;
  uint32_t tailSeq = 0;

public:
  PRF();
  uint8_t pop();
  void push(int index);
  bool isFreeListEmpty() const;
  uint32_t getHeadSeq() const;
  uint32_t getTailSeq() const;
  uint8_t getFreeListSlot(uint32_t seq) const {
    return freeList[seq & (PRF_CAP - 1)];
  }
  void restoreHead(uint32_t ckptHeadSeq);
  bool isReady(int index) const;
  int32_t getValue(int index) const;
  void write(int index, int32_t value);
  uint32_t head() const { return headSeq & (PRF_CAP - 1); }
  uint32_t tail() const { return tailSeq & (PRF_CAP - 1); }
  uint32_t size() const { return tailSeq - headSeq; }
  void tick(const PRFInput&, systemState&);
};
