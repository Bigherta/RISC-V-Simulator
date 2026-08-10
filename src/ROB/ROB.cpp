#include "../include/ROB.hpp"
#include <cstdint>
#include <stdexcept>

bool ROB::isFull() const { return ((tail + 1) & 0x3F) == head; }

bool ROB::isEmpty() const { return head == tail; }

int ROB::push(ROBEntry entry) {
  entry.seq = next_seq++;
  ROBqueue[tail] = entry;
  int index = tail;
  tail = (tail + 1) & 0x3F;
  return index;
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

uint64_t ROB::getSeq(int index) const { return ROBqueue[index].seq; }

bool ROB::isValueValidAt(int index) const {
  return ROBqueue[index].isValueValid;
}

bool ROB::isCommitReadyAt(int index) const {
  return ROBqueue[index].isCommitReady;
}

int32_t ROB::getValue(int index) const { return ROBqueue[index].value; }

ROBType ROB::getType(int index) const { return ROBqueue[index].type; }

int ROB::getDest(int index) const { return ROBqueue[index].dest; }

int32_t ROB::getPC(int index) const { return ROBqueue[index].pc; }

bool ROB::getHalt(int index) const { return ROBqueue[index].halt; }

const BranchPredictorSnapshot &ROB::getRASCkpt(int index) const {
  return ROBqueue[index].ras_ckpt;
}

const int *ROB::getRATCkpt(int index) const {
  return ROBqueue[index].rat_ckpt;
}

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
void ROB::setROBCommitReady(int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].isCommitReady = true;
}

void ROB::setROBValueValid(int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].isValueValid = true;
}

int ROB::getPredictedPC(int index) const { return ROBqueue[index].predictedPC; }

uint8_t ROB::getLsqTailSnapshot(int index) const { return ROBqueue[index].lsqTailSnapshot; }

bool ROB::isHeadCommitReady() const { return peek().isCommitReady; }
bool ROB::isHeadValueValid() const { return peek().isValueValid; }

uint64_t ROB::headSeq() const { return ROBqueue[head].seq; }

int ROB::getHead() const { return head; }

int ROB::getTail() const { return tail; }

void ROB::flush(int squashIndex) { tail = (squashIndex + 1) & 0x3F; }
