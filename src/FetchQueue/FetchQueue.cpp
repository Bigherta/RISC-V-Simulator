#include "../include/FetchQueue.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
#include <stdexcept>

bool FetchQueue::isFull() const { return ((tail + 1) & (FQ_CAP - 1)) == head; }

bool FetchQueue::isEmpty() const { return head == tail; }

void FetchQueue::push(uint32_t raw, int pc, int32_t predictedPC,
               uint8_t ckptId) {
  FetchQueueEntry entry{};
  entry.raw = raw;
  entry.pc = pc;
  entry.predictedPC = predictedPC;
  entry.ckptId = ckptId;
  FetchQueueEntries[tail] = entry;
  tail = (tail + 1) & (FQ_CAP - 1);
}

int32_t FetchQueue::headPredictedPC() const {
  if (isEmpty())
    throw std::runtime_error("headPredictedPC on empty FetchQueue!");
  return FetchQueueEntries[head].predictedPC;
}
uint8_t FetchQueue::headCkptId() const {
  if (isEmpty())
    throw std::runtime_error("headCkptId on empty FetchQueue!");
  return FetchQueueEntries[head].ckptId;
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

void FetchQueue::tick(const FQInput&input, systemState & CPUstate){
  if (input.squashDetect.needSquash) {
    CPUstate.FQModule.clear(); 
    return;
  }
  if (input.ICacheModule.isReturnReady()) {
    if (!input.haltFetched && !isFull()) {
      uint32_t raw = input.ICacheModule.returnRaw();
      int32_t pc = input.ICacheModule.returnPC();
      int32_t predPC = input.ICacheModule.returnPredictPC();
      CPUstate.FQModule.push(raw, pc, predPC, input.ICacheModule.returnCkptId());
    }
  }
  if (!isEmpty() && !input.DecodeUnitModule.isFull())
    CPUstate.FQModule.pop();
}