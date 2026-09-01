#include "../include/DCache.hpp"
#include "../include/CPU.hpp"
#include "common.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>

void DCache::snapshotFrom(const DCache &other) {
  // Only pipeline state is copied -- the line array stays outside the
  // snapshot (never memcpy'd): cacheSets is heap-sized state owned by the
  // active instance only (reorder diff compares CPUstate live values).
  busy = other.busy;
  phase = other.phase;
  currentTime = other.currentTime;
  request = other.request;
  loadBuffer = other.loadBuffer;
  cacheRequestBuffer = other.cacheRequestBuffer;
  hitCount = other.hitCount;
  missCount = other.missCount;
  writebackCount = other.writebackCount;
}

uint8_t DCache::AllocateLine(int set_idx, uint32_t tag) const {
  // TODO
  int invalidIndex = -1;
  for (int i = 0; i < cacheSets[set_idx].lines.size(); i++) {
    if (!cacheSets[set_idx].lines[i].valid && invalidIndex == -1) {
      invalidIndex = i;
    }
  }
  int targetIndex;
  if (invalidIndex != -1) {
    targetIndex = invalidIndex;
  } else {
    uint32_t min_last_use_time = cacheSets[set_idx].lines[0].lastAccessTime;
    auto evictIndex = 0;
    for (int i = 0; i < cacheSets[set_idx].lines.size(); i++) {
      if (cacheSets[set_idx].lines[i].lastAccessTime < min_last_use_time) {
        min_last_use_time = cacheSets[set_idx].lines[i].lastAccessTime;
        evictIndex = i;
      }
    }
    targetIndex = evictIndex;
  }
  return targetIndex;
}

/**
 * Processor read.
 * @param addr physical address.
 * @return data value.
 */
bool DCache::PrRd(uint32_t addr, int n_bytes, bool isSigned, int32_t &value) {
  // A request must never straddle a 16B line: offset+width within the block.
  assert((addr & (DCACHE_BLOCK_CAP - 1)) + n_bytes <= DCACHE_BLOCK_CAP);
  currentTime++;
  // TODO
  auto block_num = addr >> 4;
  auto set_index = block_num & (NUM_OF_SETS - 1);
  auto tag = addr >> DCACHE_TAG_SHIFT;
  auto &cacheSet = cacheSets[set_index];
  bool hit = false;
  uint8_t hitIndex = 0;
  for (int i = 0; i < cacheSet.lines.size(); i++) {
    if (cacheSet.lines[i].tag == tag && cacheSet.lines[i].valid) {
      hit = true;
      hitIndex = i;
    }
  }
  if (hit && cacheSet.lines[hitIndex].valid) {
    cacheSet.lines[hitIndex].lastAccessTime = currentTime;
    uint32_t rawData = 0;
    for (int i = 0; i < n_bytes; ++i) {
      rawData |= cacheSet.lines[hitIndex].datas[(addr & 0xF) + i] << (i * 8);
    }
    // sign-extend sub-word signed loads (mask branch identical to
    // DMEM::load_n_bytes): a bare static_cast<int32_t> would leave the
    // high bits zero for n<4 signed reads.
    if (isSigned && n_bytes < 4 &&
        (rawData & (1 << ((n_bytes << 3) - 1)))) {
      rawData |= ~((1 << (n_bytes << 3)) - 1);
    }
    value = static_cast<int32_t>(rawData);
    static int dbgH = 0;
    if (debug::enabled(debug::TOPIC_DCACHE) && dbgH < 8) {
      debug::print("[dc-hit] addr=%u set=%u way=%u val=%d\n", addr, set_index,
                   hitIndex, value);
      dbgH++;
    }
    return true;
  } else {
    auto distributeWay = AllocateLine(set_index, tag);
    if (!cacheRequestBuffer.valid) {
      cacheRequestBuffer.request.address = addr;
      cacheRequestBuffer.request.isSigned = isSigned;
      cacheRequestBuffer.request.n_bytes = n_bytes;
      cacheRequestBuffer.request.op = Operation::Load;
      cacheRequestBuffer.valid = true;
      cacheRequestBuffer.targetWay = distributeWay;
    }
    request.readValid = true;
    request.read.address = (addr >> 4) << 4;
    request.read.remainCycle = 3;
    if (cacheSet.lines[distributeWay].dirty) {
      request.writeValid = true;
      // Victim base address must be rebuilt from the VICTIM line's own tag,
      // not the incoming request's tag -- otherwise the dirty data lands on
      // the wrong frame and the refetch below reads back stale memory.
      request.write.address =
          ((cacheSet.lines[distributeWay].tag << DCACHE_INDEX_BITS) +
           set_index)
          << 4;
      std::memcpy(request.write.lineData, cacheSet.lines[distributeWay].datas,
                  DCACHE_BLOCK_CAP);
      request.write.remainCycle = 3; // write port latency, mirrors read
      writebackCount++; // dirty eviction: LINE_WRITE issued alongside fill
      static int dbgW = 0;
      if (debug::enabled(debug::TOPIC_DCACHE) && dbgW < 20) {
        debug::print("[dc-wb] victim=%u set=%u way=%u d0=%02x\n",
                     request.write.address, set_index, distributeWay,
                     cacheSet.lines[distributeWay].datas[0]);
        dbgW++;
      }
    }
    return false;
  }
}

/**
 * Processor write.
 * @param addr physical address.
 * @param val value to write.
 */
bool DCache::PrWr(uint32_t addr, uint32_t val, int n_bytes) {
  // A request must never straddle a 16B line: offset+width within the block.
  assert((addr & (DCACHE_BLOCK_CAP - 1)) + n_bytes <= DCACHE_BLOCK_CAP);
  currentTime++;
  // TODO
  auto block_num = addr >> 4;
  auto set_index = block_num & (NUM_OF_SETS - 1);
  auto tag = addr >> DCACHE_TAG_SHIFT;
  auto &cacheSet = cacheSets[set_index];
  bool hit = false;
  uint8_t hitIndex = 0;
  for (int i = 0; i < cacheSet.lines.size(); i++) {
    if (cacheSet.lines[i].tag == tag && cacheSet.lines[i].valid) {
      hit = true;
      hitIndex = i;
    }
  }
  int targetIndex;
  if (hit && cacheSet.lines[hitIndex].valid) {
    cacheSet.lines[hitIndex].lastAccessTime = currentTime;
    cacheSet.lines[hitIndex].dirty = true;
    for (int i = 0; i < n_bytes; ++i) {
      cacheSet.lines[hitIndex].datas[(addr & 0xF) + i] =
          (val >> (i * 8)) & ((1 << 8) - 1);
    }
    return true;
  } else {
    auto distributeWay = AllocateLine(set_index, tag);
    if (!cacheRequestBuffer.valid) {
      cacheRequestBuffer.request.address = addr;
      cacheRequestBuffer.request.n_bytes = n_bytes;
      cacheRequestBuffer.request.op = Operation::Store;
      cacheRequestBuffer.valid = true;
      cacheRequestBuffer.targetWay = distributeWay;
      cacheRequestBuffer.request.value = val;
    }
    request.readValid = true;
    request.read.address = (addr >> 4) << 4;
    request.read.remainCycle = 3;
    if (cacheSet.lines[distributeWay].dirty) {
      request.writeValid = true;
      // Victim base address must be rebuilt from the VICTIM line's own tag,
      // not the incoming request's tag -- otherwise the dirty data lands on
      // the wrong frame and the refetch below reads back stale memory.
      request.write.address =
          ((cacheSet.lines[distributeWay].tag << DCACHE_INDEX_BITS) +
           set_index)
          << 4;
      std::memcpy(request.write.lineData, cacheSet.lines[distributeWay].datas,
                  DCACHE_BLOCK_CAP);
      request.write.remainCycle = 3; // write port latency, mirrors read
      writebackCount++; // dirty eviction: LINE_WRITE issued alongside fill
      static int dbgW = 0;
      if (debug::enabled(debug::TOPIC_DCACHE) && dbgW < 20) {
        debug::print("[dc-wb] victim=%u set=%u way=%u d0=%02x\n",
                     request.write.address, set_index, distributeWay,
                     cacheSet.lines[distributeWay].datas[0]);
        dbgW++;
      }
    }
    return false;
  }
}

void DCache::tick(const DCacheInput &input, systemState &CPUstate) {
  CPUstate.DCacheModule
      .request = {}; // 双发脉冲默认无效（DMEM 靠 busy 门控防重复认领）
  CPUstate.DCacheModule
      .loadBuffer = {}; // 响应默认无效（一拍有效，LQ 每拍查 loadResp）
  CPUstate.DCacheModule.busy = busy;   // 保持
  CPUstate.DCacheModule.phase = phase; // 保持

  if (phase == Phase::READY) {
    // ---- 组合查询 + 接受派发（命中自答 / 缺失双发）
    if (input.decision.valid) {
      // Accepting a new decision requires both DMEM ports to be drained:
      // the arbiter gates on !isBusy(), and busy covers the whole miss,
      // so READY implies the previous op fully completed.
      assert(!input.DMEMModule.isReadBusy() && !input.DMEMModule.isWriteBusy());
      const auto &req = input.decision.request;
      if (req.op == Operation::Load) {
        int32_t value = 0;
        bool hit = CPUstate.DCacheModule.PrRd(req.address, req.n_bytes,
                                              req.isSigned, value);
        if (hit) {
          // 1 拍自答：loadBuffer 当拍填好（LQ 下拍 comb 读 loadResp 可见）
          CPUstate.DCacheModule.loadBuffer =
              LoadResponse{true, req.memIndex, req.robTag, value};
          CPUstate.DCacheModule.hitCount++;
        } else {
          // PrRd parks without identity (address/width/sign only); the
          // request's LQ identity lives on the decision bus -- latch it here
          // so the FILL_WAIT serve stage can build the real LoadResponse.
          CPUstate.DCacheModule.cacheRequestBuffer.request.memIndex =
              req.memIndex;
          CPUstate.DCacheModule.cacheRequestBuffer.request.robTag = req.robTag;
          CPUstate.DCacheModule.busy = true;
          CPUstate.DCacheModule.phase = Phase::WAIT;
          CPUstate.DCacheModule.missCount++;
        }
      } else { /* Store 同构：PrWr 命中无 loadBuffer / 缺失 busy+FILL_WAIT */
        bool hit =
            CPUstate.DCacheModule.PrWr(req.address, req.value, req.n_bytes);
        if (hit) {
          CPUstate.DCacheModule.hitCount++;
        } else {
          CPUstate.DCacheModule.cacheRequestBuffer.request.memIndex =
              req.memIndex;
          CPUstate.DCacheModule.cacheRequestBuffer.request.robTag = req.robTag;
          CPUstate.DCacheModule.busy = true;
          CPUstate.DCacheModule.phase = Phase::WAIT;
          CPUstate.DCacheModule.missCount++;
        }
      }
    }
  } else { // Phase::FILL_WAIT —— if/else 互斥，与 READY 写集不冲突
    // ---- 下拍可见：DMEM 完成于 tick N（写活值）→ comb N+1 快照 → 本拍读到
    if (input.DMEMModule.isReplyReady() && !input.DMEMModule.isWriteBusy()) {
      auto targetWay = cacheRequestBuffer.targetWay;
      auto addr = cacheRequestBuffer.request.address;
      auto block_num = addr >> 4;
      auto set_index = block_num & (NUM_OF_SETS - 1);
      auto tag = addr >> DCACHE_TAG_SHIFT;
      // 2) 填行：memcpy 行数据 +
      // valid/tag/dirty=false/lastAccessTime=currentTime
      const auto &output = input.DMEMModule.reply();
      memcpy(CPUstate.DCacheModule.cacheSets[set_index].lines[targetWay].datas,
             output.lineData, sizeof(output.lineData));
      CPUstate.DCacheModule.cacheSets[set_index].lines[targetWay].dirty = false;
      CPUstate.DCacheModule.cacheSets[set_index].lines[targetWay].tag = tag;
      CPUstate.DCacheModule.cacheSets[set_index].lines[targetWay].valid = true;
      CPUstate.DCacheModule.cacheSets[set_index]
          .lines[targetWay]
          .lastAccessTime = currentTime;
      static int dbgF = 0;
      if (debug::enabled(debug::TOPIC_DCACHE) && dbgF < 8) {
        debug::print("[dc-fill] addr=%u set=%u way=%u d0=%02x\n", addr,
                     set_index, targetWay, output.lineData[0]);
        dbgF++;
      }
      // 3) 服务 park：load →
      // 提取字节填 loadBuffer（符号扩展照抄 DMEM::load_n_bytes）；
      //    store → 逐字节写行 + dirty=true
      if (cacheRequestBuffer.request.op == Operation::Store) {
        CPUstate.DCacheModule.cacheSets[set_index].lines[targetWay].dirty =
            true;
        for (int i = 0; i < cacheRequestBuffer.request.n_bytes; ++i) {
          CPUstate.DCacheModule.cacheSets[set_index]
              .lines[targetWay]
              .datas[(addr & 0xF) + i] =
              (cacheRequestBuffer.request.value >> (i * 8)) & ((1 << 8) - 1);
        }
        static int dbgS = 0;
        if (debug::enabled(debug::TOPIC_DCACHE) && dbgS < 8) {
          debug::print("[dc-svcS] addr=%u val=%u nb=%d\n", addr,
                       cacheRequestBuffer.request.value,
                       cacheRequestBuffer.request.n_bytes);
          dbgS++;
        }
      } else {
        int32_t result = 0;
        for (int i = 0; i < cacheRequestBuffer.request.n_bytes; ++i) {
          auto byte_data = output.lineData[(addr & 0xF) + i];
          result |= (byte_data << (i << 3));
          if (i == cacheRequestBuffer.request.n_bytes - 1 &&
              cacheRequestBuffer.request.n_bytes < 4 &&
              cacheRequestBuffer.request.isSigned) {
            if (result &
                (1 << ((cacheRequestBuffer.request.n_bytes << 3) - 1))) {
              auto mask =
                  ~((1 << (cacheRequestBuffer.request.n_bytes << 3)) - 1);
              result |= mask;
            }
          }
        }
        // 写自有纪律：loadBuffer 是 DCache 自己的状态，写活值
        // CPUstate.DCacheModule（tick 开头已默认清空，此处覆盖为有效）。
        CPUstate.DCacheModule.loadBuffer.value = result;
        CPUstate.DCacheModule.loadBuffer.memIndex =
            cacheRequestBuffer.request.memIndex;
        CPUstate.DCacheModule.loadBuffer.robTag =
            cacheRequestBuffer.request.robTag;
        CPUstate.DCacheModule.loadBuffer.valid = true;
        static int dbgL = 0;
        if (debug::enabled(debug::TOPIC_DCACHE) && dbgL < 8) {
          debug::print("[dc-svcL] addr=%u val=%d\n", addr, result);
          dbgL++;
        }
      }
      // 4) 清 park、request 已默认清、busy=false、phase=READY
      // 写自有纪律：全部落到 CPUstate.DCacheModule（活值）；下拍 comb
      // 快照刷新后仲裁器/LQ 才观察到，时序不变量不变。
      CPUstate.DCacheModule.cacheRequestBuffer = DCachePark{};
      CPUstate.DCacheModule.request = DMEMRequest{};
      CPUstate.DCacheModule.busy = false;
      CPUstate.DCacheModule.phase = Phase::READY;
    } // else: 条件未满足 → 保持
  }
}