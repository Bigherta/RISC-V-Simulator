#pragma once
#include <cstring>
#include "common.hpp"
#include <cstdint>
struct BRU;
struct ROB;
struct systemState;

// BP update arbitration input: both table-training sources (BRU branch results
// and CDB JAL/JALR transfers) converge to this single point so that the two
// update calls keep a fixed order (BRU candidate first) regardless of stage
// scheduling order.
struct BPUpdateInput {
  const BRU &BRUModule;
  CDBOutput cdbOut;
  const ROB &ROBModule;
  SquashInfo squashDetect;
  FetchDecision fetchDecision;
  BPUpdateInput(const BRU &bru, const ROB &rob)
      : BRUModule(bru), ROBModule(rob) {}
};

// SARAS correction queue entry: the address, its LIFO position, and the
// times counter before the speculative action (so both pops and
// times inc/dec are undoable). One entry is recorded for every
// speculative call-dedup and every speculative ret.
struct AlignEntry {
  uint32_t addr;
  uint8_t index;
  uint32_t times;
};

struct RASEntry{
  uint32_t retPC;
  uint32_t times;
};

class BPU {
private:
  struct Cand {
    bool valid = false;
    int32_t pc = 0;
    bool taken = false;
    int32_t target = 0;
    uint16_t ghr = 0;
    bool cond = true;
    bool isCall = false;
    bool isRet = false;
  };
  uint8_t globalPHT[PC_Direct_CAP] = {};
  uint8_t LHT[LHT_CAP] = {};
  uint8_t localPHT[LOCAL_PHT_CAP] = {};
  uint8_t selector[SELECTOR_CAP] = {};
  BTBEntry BTB[BTB_CAP] = {};
  // SARAS: ring return-address stack (RAS) with per-entry recursion
  // counter (times) + correction queue (AlignQueue). All three ring
  // counters are uint8_t and wrap at 256 (safe: in-flight <64, and
  // ALIGNQ_CAP/RAS_CAP=16).
  RASEntry RAS[RAS_CAP] = {};
  uint8_t RAS_top = 0;              // ring write pointer (wraps at 256)
  AlignEntry alignQueue[ALIGNQ_CAP] = {};
  uint8_t alignHead = 0;            // AlignQueue head (advanced at commit)
  uint8_t alignTail = 0;            // AlignQueue tail (appended on CALL-dedup / RET)
  uint16_t GHR = 0;
  BPUSnapshot bpCkpt[CKPT_CAP] = {};
  uint8_t nextCkptId = 0;
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;

public:
  BPU() {
    std::memset(globalPHT, 1, sizeof(globalPHT));
    std::memset(localPHT, 1, sizeof(localPHT));
    std::memset(selector, 1, sizeof(selector));
  }
  uint64_t getBranchTotal() const { return branchTotal; }
  uint64_t getBranchCorrect() const { return branchCorrect; }
  PredictInfo predict(int32_t pc) const;
  void update(int32_t pc, bool taken, int32_t target, uint16_t ghr);
  void updateJump(int32_t pc, int32_t target, bool isCall, bool isRet);
  void shiftGHR(bool taken);
  BPUSnapshot snapshotCheckPoint() const;
  void recoverCheckPoint(const BPUSnapshot &);
  uint8_t getNextCkptId() const { return nextCkptId; }
  void tick(const BPUpdateInput &, systemState &);
};
