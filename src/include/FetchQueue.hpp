#pragma once
#include "common.hpp"
#include <cstdint>
#include <cstring>
struct FetchQueueEntry {
  uint32_t raw;
  int pc;
  int32_t predictedPC;
  BranchPredictorSnapshot BPsnapshot;
};
struct IMEM;
struct DecodeUnit;
struct FQInput {
  SquashInfo squashDetect;
  const IMEM &IMEMModule;
  const DecodeUnit &DecodeUnitModule;
  bool haltFetched;
  FQInput(const IMEM &imem, const DecodeUnit &du)
      : IMEMModule(imem), DecodeUnitModule(du) {}
};
struct systemState;
class FetchQueue {
  friend struct ReorderTester;

private:
  FetchQueueEntry FetchQueueEntries[FQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  FetchQueue() { std::memset(this, 0, sizeof(*this)); }
  bool isFull() const;
  bool isEmpty() const;

  void push(uint32_t raw, int pc, int32_t predictedPC,
            const BranchPredictorSnapshot &ckpt);
  uint32_t headRaw() const;
  int headpc() const;
  int32_t headPredictedPC() const;
  BranchPredictorSnapshot headBPSnapshot() const;
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  void clear();
  void tick(const FQInput &, systemState &);
};
