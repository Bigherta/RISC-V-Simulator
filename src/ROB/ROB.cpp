#include "../include/ROB.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

bool ROB::isFull() const { return ((tail + 1) & 0x3F) == head; }

bool ROB::isEmpty() const { return head == tail; }

int ROB::push(ROBEntry entry) {
  entry.tag = ROB_Tag;
  ROBqueue[tail] = entry;
  tail = (tail + 1) & 0x3F;
  return ROB_Tag++;
}

ROBEntry ROB::pop() {
  auto temp = ROBqueue[head];
  head = (head + 1) & 0x3F;
  return temp;
}

ROBEntry ROB::peek() const {
  if (isEmpty())
    throw std::runtime_error("peek an empty ROB!");
  return ROBqueue[head];
}

int ROB::getIndex(int Tag) const {
  for (int i = head; i != tail; i = (i + 1) & 0x3F) {
    if (ROBqueue[i].tag == Tag) {
      return i;
    }
  }
  return -1;
}

ROBEntry ROB::getEntry(int index) const { return ROBqueue[index]; }

void ROB::writeROBValue(int32_t value, int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].value = value;
}
void ROB::writeROBPredictedPC(uint32_t pc, int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].predictedPC = pc;
}
void ROB::writeROBState(ROBState state, int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].state = state;
}

int ROB::getPredictedPC(int index) const { return ROBqueue[index].predictedPC; }

int ROB::ClearRATDest() const { return peek().dest; }

int ROB::headROB() const { return peek().tag; }

ROBState ROB::headState() const { return peek().state; }

uint64_t ROB::currentTag() const { return ROB_Tag; }

int ROB::getHead() const { return head; }

int ROB::getTail() const { return tail; }

void ROB::flush(int tag) {
  int first_flushed = -1;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (ROBqueue[cur].tag > tag) {
      if (first_flushed == -1)
        first_flushed = cur;
    }
  }
  if (first_flushed != -1)
    tail = first_flushed;
}