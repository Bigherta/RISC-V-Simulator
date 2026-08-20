#pragma once
#ifndef DMEM_HPP
#define DMEM_HPP
#include "../include/Memory.hpp"
#include "../include/common.hpp"

struct systemState;

struct DMEMInput {
  SquashInfo squashDetect;
  MemDispatchDecision decision;
  DMEMInput() = default;
};

// Data memory.
class DMEM : public Memory {
  friend struct ReorderTester;
  bool busy = false;
  bool bufferValid = false;
  MemRequest MemExecution = {};
  MemRequest MemOutputBuffer = {};
  uint64_t speculativeLoads = 0;  // naive 投机派发计数（诊断）

public:
  DMEM() = default;
  DMEM(const Memory& mem) : Memory(mem) {}
  DMEM(const DMEM&) = default;
  DMEM& operator=(const DMEM&) = default;

  void snapshotFrom(const DMEM& other);
  int32_t load_n_bytes(uint32_t address, int n, bool isSigned,
                       bool tolerant) const;
  void store_n_bytes(uint32_t address, int value, int n);
  void MemPull();
  LoadResponse LoadReturn(const SquashInfo& squash) const;
  bool isBusy() const;
  bool isReady() const;
  uint64_t getSpeculativeLoads() const { return speculativeLoads; }
  void tick(const DMEMInput&, systemState&);
};
#endif // DMEM_HPP
