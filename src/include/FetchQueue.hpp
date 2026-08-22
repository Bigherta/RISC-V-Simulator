#pragma once
#include "common.hpp"
#include <cstdint>
#include <cstring>
struct FetchQueueEntry {
  uint32_t raw;
  int pc;
  int32_t predictedPC;
  uint8_t ckptId;
};
struct ICache;
struct DecodeUnit;
struct FQInput {
  SquashInfo squashDetect;
  const ICache &ICacheModule;
  const DecodeUnit &DecodeUnitModule;
  bool haltFetched;
  FQInput(const ICache &icache, const DecodeUnit &du)
      : ICacheModule(icache), DecodeUnitModule(du) {}
};
struct systemState;
class FetchQueue {
  friend struct ReorderTester;

private:
  FetchQueueEntry FetchQueueEntries[FQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;
  void push(uint32_t raw, int pc, int32_t predictedPC, uint8_t ckptId);
  void pop();
  void clear();

public:
  FetchQueue() { std::memset(this, 0, sizeof(*this)); }
  bool isFull() const;
  bool isEmpty() const;
  uint32_t headRaw() const;
  int headpc() const;
  int32_t headPredictedPC() const;
  uint8_t headCkptId() const;
  uint8_t getHead() const;
  uint8_t getTail() const;
  void tick(const FQInput &, systemState &);
};
