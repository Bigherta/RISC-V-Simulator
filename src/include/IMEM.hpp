#pragma once
#include "BranchPredictor.hpp"
#ifndef IMEM_HPP
#define IMEM_HPP
#include "../include/Memory.hpp"
#include "../include/common.hpp"
#include <cstdint>

struct systemState;
struct IMEMInput {
  const SquashInfo squashDetect;
  const bool FQready;
  const BranchPredictor &BPModule;
  const int32_t PC;
  const bool haltFetchRequest;
};
struct IMEMRequest {
  uint32_t raw_inst;
  int remain_cycle = 3;
  int32_t PC;
  int32_t predictPC;
  bool valid = true;
};
// Instruction memory.
class IMEM : public Memory {
private:
  IMEMRequest IMEMBuffer[IMEM_CAP];
  uint8_t head;
  uint8_t count;

public:
  IMEM() = default;
  IMEM(const Memory &mem) : Memory(mem) {}
  IMEM(const IMEM &) = default;
  IMEM &operator=(const IMEM &) = default;

  inline uint32_t read_inst(uint32_t pc) const {
    return static_cast<uint32_t>(read_data(pc)) |
           (static_cast<uint32_t>(read_data(pc + 1)) << 8) |
           (static_cast<uint32_t>(read_data(pc + 2)) << 16) |
           (static_cast<uint32_t>(read_data(pc + 3)) << 24);
  }
  void clear();
  void push();
  void work(const IMEMInput&, systemState&);
};
#endif // IMEM_HPP
