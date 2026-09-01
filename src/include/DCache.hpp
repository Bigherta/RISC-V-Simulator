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
  uint32_t lastAccessTime = 0;
};
struct CacheSet {
  std::array<Cacheline, NUM_OF_WAYS> lines;
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

// Step 0: pure-forwarding skeleton between MemRequestArbiter and DMEM.
//
// No line array, no hit/miss logic yet -- every accepted decision is forwarded
// verbatim to DMEM (Operation::Load / Operation::Store, untouched) and the
// DMEM completion is relayed back to the LQ one cycle later.
//
// Hard constraints locked in here (see plan §2):
//  * isBusy() == "a request is in flight" (semantics equivalent to the old
//    dmem.isBusy() gate) -- the arbiter never issues while busy.
//  * When !isBusy() the DCache must UNCONDITIONALLY accept the decision: the
//    store was already popped from the SQ in this cycle.
//  * The line array NEVER lives in the comb snapshot (that arrives in Step 1);
//    comb only reads isBusy()/forwardRequest() -- no hit() predicate.
class DCache {
  friend struct ReorderTester;

private:
  enum class Phase : uint8_t { READY, WAIT };
  bool busy = false;
  Phase phase = Phase::READY;
  uint32_t currentTime = 0;
  std::array<CacheSet, NUM_OF_SETS> cacheSets;
  DCachePark cacheRequestBuffer;
  LoadResponse loadBuffer;
  DMEMRequest request;
  // Hit/miss counters (VERBOSE=dcache). Pure accumulators, mirrored by
  // snapshotFrom so the reorder diff observes them consistently.
  uint32_t hitCount = 0;
  uint32_t missCount = 0;
  uint32_t writebackCount = 0; // dirty-line evictions (LINE_WRITE issued)

public:
  bool isBusy() const { return busy; }
  uint32_t getHitCount() const { return hitCount; }
  uint32_t getMissCount() const { return missCount; }
  uint32_t getWritebackCount() const { return writebackCount; }
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
