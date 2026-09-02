#include "../include/ICache.hpp"
#include "../include/CPU.hpp"
#include <cstdint>

void ICache::clear() {
  // only clear the request queue; cache lines are non-speculative and kept
  for (int i = 0; i < REQUEST_CAP; ++i) requestBuffer[i].valid = false;
  head = 0;
  count = 0;
}

void ICache::pop() {
  requestBuffer[head].valid = false;
  head = (head + 1) & (REQUEST_CAP - 1);
  --count;
}

void ICache::pushRequest(uint32_t raw_inst, uint32_t pc, int32_t predictPC,
                         uint8_t ckptId, bool valid) {
  ICacheRequest request{};
  request.raw_inst = raw_inst;
  request.PC = pc;
  request.predictPC = predictPC;
  request.ckptId = ckptId;
  request.valid = valid;
  requestBuffer[(head + count) & (REQUEST_CAP - 1)] = request;
  ++count;
}

bool ICache::hit(uint32_t addr) const {
  auto index = (addr >> 4) & (ICACHE_CAP - 1);
  return blocks[index].valid && (blocks[index].tag == (addr >> 13));
}

void ICache::tick(const ICacheInput &input, systemState &CPUstate) {
  // stage 3 flush: clear the speculative request queue (cache lines kept)
  if (input.squashDetect.needSquash) {
    CPUstate.ICacheModule.clear();
    return;
  }
  // stage 2 pop: self-release once FQ consumed the ICache head (write-own-only)
  if (input.popConsume) {
    CPUstate.ICacheModule.pop();
  }
  // stage 2 line refill: consume the IMEM line-return bus (word4), fill the
  // cache line and backfill the placeholder entry
  if (input.lineReturn.valid) {
    auto cachelineIndex = (input.lineReturn.lineAddr >> 4) & (ICACHE_CAP - 1);
    CPUstate.ICacheModule.blocks[cachelineIndex].valid = true;
    CPUstate.ICacheModule.blocks[cachelineIndex].tag = input.lineReturn.lineAddr >> 13;
    for (int w = 0; w < 4; ++w) {
      CPUstate.ICacheModule.blocks[cachelineIndex].data[w] =
          input.lineReturn.data[w];
    }
    // backfill placeholder: head now points to the placeholder awaiting this
    // line (already past pop); only refill when the queue is non-empty and the
    // head entry is still an invalid placeholder
    if (CPUstate.ICacheModule.count > 0 &&
        !CPUstate.ICacheModule.requestBuffer[CPUstate.ICacheModule.head].valid) {
      uint32_t pc = CPUstate.ICacheModule.requestBuffer[CPUstate.ICacheModule.head].PC;
      uint32_t word = input.lineReturn.data[(pc >> 2) & 3];
      CPUstate.ICacheModule.requestBuffer[CPUstate.ICacheModule.head].raw_inst = word;
      CPUstate.ICacheModule.requestBuffer[CPUstate.ICacheModule.head].valid = true;
    }
  }
  // stage 1 push new request: enqueue on a valid fetchDecision (hit -> valid,
  // miss -> placeholder)
  if (input.fetchDecision.valid) {
    bool isHit = hit(input.fetchDecision.pc);
    uint32_t raw_inst = 0;
    if (isHit) {
      auto cachelineIndex = (input.fetchDecision.pc >> 4) & (ICACHE_CAP - 1);
      raw_inst = blocks[cachelineIndex].data[(input.fetchDecision.pc >> 2) & 3];
    }
    CPUstate.ICacheModule.pushRequest(raw_inst, input.fetchDecision.pc,
        input.fetchDecision.predictedPC, input.fetchDecision.ckptId, isHit);
    if (isHit) CPUstate.ICacheModule.hitCount++;
    else CPUstate.ICacheModule.missCount++;
  }
}