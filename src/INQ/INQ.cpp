#include "../include/INQ.hpp"
#include "../include/Decoder.hpp"
#include <cstdint>
#include <stdexcept>

bool INQ::isFull() const { return ((tail + 1) & (INQ_CAP - 1)) == head; }

bool INQ::isEmpty() const { return head == tail; }

void INQ::push(uint32_t raw, int pc, int32_t predictedPC,
               const BranchPredictorSnapshot &ckpt) {
  INQEntry entry{};
  entry.raw = raw;
  entry.pc = pc;
  entry.predictedPC = predictedPC;
  entry.ras_ckpt = ckpt;
  INQqueue[tail] = entry;
  tail = (tail + 1) & (INQ_CAP - 1);
}

int32_t INQ::peekPredictedPC() const {
  if (isEmpty())
    throw std::runtime_error("peekPredictedPC an empty INQ!");
  return INQqueue[head].predictedPC;
}
BranchPredictorSnapshot INQ::peekRASCkpt() const {
  if (isEmpty())
    throw std::runtime_error("peekCheckPoint an empty INQ!");
  return INQqueue[head].ras_ckpt;
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

int INQ::decodeDetect() const {
  if (isEmpty())
    return -1;
  for (int i = head; i != tail; i = (i + 1) & (INQ_CAP - 1)) {
    if (!INQqueue[i].decoded) {
      return i;
    }
  }
  return -1;
}

void INQ::decode(int index) {
  INQqueue[index].ninst = Decoder::decode(INQqueue[index].raw);
  INQqueue[index].ninst.pc = INQqueue[index].pc;
  INQqueue[index].decoded = true;
  INQqueue[index].ninst.isHalt = (INQqueue[index].raw == 0x0ff00513);
}

bool INQ::headDecoded() const { return !isEmpty() && INQqueue[head].decoded; }

INQEntry INQ::getEntry(int index) const { return INQqueue[index]; }

void INQ::clear() {
  std::memset(this, 0, sizeof(*this));
  head = tail = 0;
}