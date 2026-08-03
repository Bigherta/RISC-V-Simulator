#include "../include/INQ.hpp"
#include "../include/Decoder.hpp"
#include <cstdint>
#include <stdexcept>

bool INQ::isFull() const { return ((tail + 1) & (INQ_CAP - 1)) == head; }

bool INQ::isEmpty() const { return head == tail; }

void INQ::push(uint32_t raw, int pc) {
  INQEntry entry{};
  entry.raw = raw;
  entry.pc = pc;
  INQqueue[tail] = entry;
  tail = (tail + 1) & (INQ_CAP - 1);
}

Instruct INQ::peek() const {
  if (isEmpty())
    throw std::runtime_error("peek an empty INQ!");
  return INQqueue[head].ninst;
}

Instruct INQ::pop() {
  auto temp = INQqueue[head].ninst;
  head = (head + 1) & (INQ_CAP - 1);
  return temp;
}

uint8_t INQ::getHead() const { return head; }

uint8_t INQ::getTail() const { return tail; }

uint8_t INQ::decodeDetect() const {
  if (isEmpty())
    return -1;
  for (int i = head; i != tail; i = (i + 1) & (INQ_CAP - 1)) {
    if (!INQqueue[i].decoded) {
      return i;
    }
  }
  return -1;
}

void INQ::decode(uint8_t index) {
  INQqueue[index].ninst = Decoder::decode(INQqueue[index].raw);
  INQqueue[index].ninst.pc = INQqueue[index].pc;
  INQqueue[index].decoded = true;
  INQqueue[index].ninst.isHalt = (INQqueue[index].raw == 0x0ff00513);
}

bool INQ::headDecoded() const { return !isEmpty() && INQqueue[head].decoded; }

void INQ::decodeAll() {
  for (int i = head; i != tail; i = (i + 1) & (INQ_CAP - 1)) {
    if (!INQqueue[i].decoded) {
      INQqueue[i].ninst = Decoder::decode(INQqueue[i].raw);
      INQqueue[i].ninst.pc = INQqueue[i].pc;
      INQqueue[i].decoded = true;
      INQqueue[i].ninst.isHalt = (INQqueue[i].raw == 0x0ff00513);
    }
  }
}

void INQ::clear() {
  std::memset(this, 0, sizeof(*this));
  head = tail = 0;
}