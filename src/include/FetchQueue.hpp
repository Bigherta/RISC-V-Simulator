#pragma once
#ifndef FETCHQUEUE_HPP
#define FETCHQUEUE_HPP
#include "common.hpp"
#include <cstdint>
#include <cstring>
struct FetchQueueEntry {
  uint32_t raw;
  int pc;
  int32_t predictedPC;
  BranchPredictorSnapshot BPsnapshot;
};

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
};
#endif // FETCHQUEUE_HPP
