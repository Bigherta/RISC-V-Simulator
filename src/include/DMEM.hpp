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
  void store_n_bytes(uint32_t address, int value, int n);
  void MemPull();

public:
  DMEM() = default;
  DMEM(const Memory &mem) : Memory(mem) {}
  DMEM(const DMEM &) = default;
  DMEM &operator=(const DMEM &) = default;
  int32_t load_n_bytes(uint32_t address, int n, bool isSigned) const;
  LoadResponse LoadReturn(const SquashInfo &squash) const;
  bool isBusy() const;
  bool isReady() const;
  void tick(const DMEMInput &, systemState &);
  void snapshotFrom(const DMEM &other);
};
#endif // DMEM_HPP
