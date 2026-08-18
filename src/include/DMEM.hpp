#pragma once
#ifndef DMEM_HPP
#define DMEM_HPP
#include "../include/Memory.hpp"
#include "../include/LSQ.hpp"
#include "../include/common.hpp"

struct systemState;

struct DMEMInput {
  SquashInfo squashDetect;
  MemDispatchDecision decision;
  const LSQ& LSQModule;
  DMEMInput(const LSQ &lsq) : LSQModule(lsq) {}
};

// Data memory.
class DMEM : public Memory {
  friend struct ReorderTester;
  bool busy = false;
  bool bufferValid = false;
  MemRequest MemExecution = {};
  MemRequest MemOutputBuffer = {};

public:
  DMEM() = default;
  DMEM(const Memory& mem) : Memory(mem) {}
  DMEM(const DMEM&) = default;
  DMEM& operator=(const DMEM&) = default;

  void snapshotFrom(const DMEM& other);
  int32_t load_n_bytes(uint32_t address, int n, bool isSigned) const;
  void store_n_bytes(uint32_t address, int value, int n);
  void MemPull();
  LoadResponse LoadReturn(const SquashInfo& squash) const;
  bool isBusy() const;
  bool isReady() const;
  void tick(const DMEMInput&, systemState&);
};
#endif // DMEM_HPP
