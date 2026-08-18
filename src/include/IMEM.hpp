#pragma once
#include "BranchPredictor.hpp"
#include "Memory.hpp"
#include "FetchQueue.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState;
struct IMEMInput {
  SquashInfo squashDetect;
  const FetchQueue &FQModule;
  FetchDecision fetchDecision;
  IMEMInput(const FetchQueue &fq) : FQModule(fq) {}
};
struct IMEMRequest {
  uint32_t raw_inst;
  int remain_cycle = 0;
  int32_t PC;
  int32_t predictPC;
  BranchPredictorSnapshot ckpt;
  bool valid = false;
};
// Instruction memory. 取指单元：除访存管线外，还拥有 fetch 控制状态
// （programCounter 推进 / haltFetched 置位，见 docs §3.28）。
class IMEM : public Memory {
private:
  IMEMRequest IMEMreqs[IMEM_CAP];
  uint8_t head = 0;
  uint8_t count = 0;
  uint32_t programCounter = 0;
  bool haltFetched = false;

public:
  IMEM() { std::memset(IMEMreqs, 0, sizeof(IMEMreqs)); }
  IMEM(const Memory &mem) : Memory(mem) {
    std::memset(IMEMreqs, 0, sizeof(IMEMreqs));
  }
  IMEM(const IMEM &) = default;
  IMEM &operator=(const IMEM &) = default;

  inline uint32_t read_inst(uint32_t pc) const {
    return static_cast<uint32_t>(read_data(pc)) |
           (static_cast<uint32_t>(read_data(pc + 1)) << 8) |
           (static_cast<uint32_t>(read_data(pc + 2)) << 16) |
           (static_cast<uint32_t>(read_data(pc + 3)) << 24);
  }
  void clear();
  void snapshotFrom(const IMEM &other);
  uint8_t getHead() const { return head; }
  uint32_t getPC() const { return programCounter; }
  bool isHaltFetched() const { return haltFetched; }
  bool isRequestFull() const { return count == IMEM_CAP; }
  bool isReturnReady() const {
    return count > 0 && IMEMreqs[head].valid &&
           IMEMreqs[head].remain_cycle == 0;
  }
  uint32_t returnRaw() const { return IMEMreqs[head].raw_inst; }
  int32_t returnPC() const { return IMEMreqs[head].PC; }
  int32_t returnPredictPC() const { return IMEMreqs[head].predictPC; }
  BranchPredictorSnapshot returnCkpt() const { return IMEMreqs[head].ckpt; }
  void pop();
  void pushRequest(uint32_t pc, int32_t predictPC,
                   const BranchPredictorSnapshot &);
  void tick(const IMEMInput &, systemState &);
};