#include "../include/FetchQueue.hpp"
#include <cstdint>
#include <stdexcept>

bool FetchQueue::isFull() const { return ((tail + 1) & (FQ_CAP - 1)) == head; }

bool FetchQueue::isEmpty() const { return head == tail; }

void FetchQueue::push(uint32_t raw, int pc, int32_t predictedPC,
               const BranchPredictorSnapshot &ckpt) {
  FetchQueueEntry entry{};
  entry.raw = raw;
  entry.pc = pc;
  entry.predictedPC = predictedPC;
  entry.BPsnapshot = ckpt;
  FetchQueueEntries[tail] = entry;
  tail = (tail + 1) & (FQ_CAP - 1);
}

int32_t FetchQueue::headPredictedPC() const {
  if (isEmpty())
    throw std::runtime_error("headPredictedPC on empty FetchQueue!");
  return FetchQueueEntries[head].predictedPC;
}
BranchPredictorSnapshot FetchQueue::headBPSnapshot() const {
  if (isEmpty())
    throw std::runtime_error("headRASCkpt on empty FetchQueue!");
  return FetchQueueEntries[head].BPsnapshot;
}

uint32_t FetchQueue::headRaw() const {
  if (isEmpty())
    throw std::runtime_error("headRaw on empty FetchQueue!");
  return FetchQueueEntries[head].raw;
}

int FetchQueue::headpc() const {
  if (isEmpty())
    throw std::runtime_error("headpc on empty FetchQueue!");
  return FetchQueueEntries[head].pc;
}

void FetchQueue::pop() { head = (head + 1) & (FQ_CAP - 1); }


uint8_t FetchQueue::getHead() const { return head; }

uint8_t FetchQueue::getTail() const { return tail; }


// index-based getters removed; use head* accessors for head entry

void FetchQueue::clear() {
  std::memset(this, 0, sizeof(*this));
  head = tail = 0;
}