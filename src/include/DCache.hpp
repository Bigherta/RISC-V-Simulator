#pragma once
#include "DMEM.hpp"
#include "ROB.hpp"
#include "common.hpp"
#include <array>
#include <cstdint>

struct systemState;
struct DCachePark {
  MemRequest request; // op/addr/value/n_bytes/isSigned/robTag/memIndex
  bool valid = false;
  uint8_t targetWay = 0; // AllocateLine 结果
};
struct Cacheline {
  bool valid = false;
  bool dirty = false;
  uint8_t datas[DCACHE_BLOCK_CAP];
  uint32_t tag = 0;
};
struct CacheSet {
  std::array<Cacheline, NUM_OF_WAYS> lines;
  uint8_t plru = 0; // tree-PLRU: b2=root, b1=left, b0=right; 0=left,1=right
};
struct DCacheInput {
  SquashInfo squashDetect;
  MemDispatchDecision decision;
  // Snapshot reference to the downstream memory: the DCache observes DMEM's
  // completion through the comb-refreshed snapshot (order-independent under
  // reorder_test; DMEM::tick only ever writes its own CPUstate members).
  const DMEM &DMEMModule;
  DCacheInput(const DMEM &dmem) : DMEMModule(dmem) {}
};

// 64KB 4-way 16B line / LRU / write-back + write-allocate / dual-port DMEM
// Hard constraints:
//  * isBusy() == "a request is in flight" -- arbiter never issues while busy.
//  * When !isBusy() the DCache must UNCONDITIONALLY accept the decision: the
//    store was already popped from the SQ in this cycle.
//  * The line array NEVER lives in the comb snapshot; comb only reads isBusy()/forwardRequest().
class DCache {
  friend struct ReorderTester;

private:
  enum class Phase : uint8_t { READY, WAIT };
  bool busy = false;
  Phase phase = Phase::READY;
  std::array<CacheSet, NUM_OF_SETS> cacheSets;
  DCachePark cacheRequestBuffer;
  LoadResponse loadBuffer;
  DMEMRequest request;

public:
  bool isBusy() const { return busy; }
  const DMEMRequest &forwardRequest() const { return request; }
  bool PrRd(uint32_t addr, int n_bytes, bool isSigned, int32_t &value);
  bool PrWr(uint32_t addr, uint32_t val, int n_bytes);
  LoadResponse loadResp(const SquashInfo &squash) const {
    LoadResponse resp{};
    if (loadBuffer.valid &&
        (!squash.needSquash ||
         ROB::isOlder(loadBuffer.robTag, squash.SquashTag))) {
      resp = loadBuffer;
    }
    return resp;
  }
  uint8_t AllocateLine(int set_idx, uint32_t tag)
      const; // distribute the line without modifying anything
  void tick(const DCacheInput &, systemState &);
  void snapshotFrom(const DCache &other);
};
