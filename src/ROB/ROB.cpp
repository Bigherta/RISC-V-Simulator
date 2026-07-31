#include "../include/ROB.hpp"
#include <stdexcept>
#include <string>

bool ROB::isFull() const { return ((tail + 1) & 0x3F) == head; }

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

ROBEntry ROB::peek() const { return ROBqueue[head]; }

int ROB::getIndex(int Tag) const {
  for (int i = head; i != tail; i = (i + 1) & 0x3F) {
    if (ROBqueue[i].tag == Tag) {
      return i;
    }
  }
  throw std::runtime_error("No finding Tag " + std::to_string(Tag));
}

ROBEntry ROB::getEntry(int Tag) const {
  for (int i = head; i != tail; i = (i + 1) & 0x3F) {
    if (ROBqueue[i].tag == Tag) {
      return ROBqueue[i];
    }
  }
  throw std::runtime_error("No finding Tag " + std::to_string(Tag));
}

void ROB::writeROB(int32_t value, int index, ROBState state) {
  ROBqueue[index].value = value;
  ROBqueue[index].state = state;
}