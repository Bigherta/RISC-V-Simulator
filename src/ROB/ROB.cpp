#include "../include/ROB.hpp"
#include <stdexcept>
#include <string>

bool ROB::isFull() { return ((tail + 1) & 0x3F) == head; }

int ROB::push(ROBEntry entry) {
  tail = (tail + 1) & 0x3F;
  entry.tag = ROB_Tag;
  ROBqueue[tail] = entry;
  return ROB_Tag++;
}

ROBEntry ROB::pop() {
  auto temp = ROBqueue[(head + 1) & 0x3F];
  head = (head + 1) & 0x3F;
  return temp;
}

ROBEntry ROB::peek() { return ROBqueue[(head + 1) & 0x3F]; }

int ROB::getIndex(int Tag) {
  for (int i = 0; i < 65; i++) {
    if (ROBqueue[i].tag == Tag) {
      return i;
    }
  }
  throw std::runtime_error("No finding Tag " + std::to_string(Tag));
}

void ROB::writeROB(int32_t value, int index, ROBState state) {
  ROBqueue[index].value = value;
  ROBqueue[index].state = state;
}