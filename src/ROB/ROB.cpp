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

bool ROB::isCommitReadyAt(int index) const {
  return ROBqueue[index].isCommitReady;
}

ROBType ROB::getType(int index) const { return ROBqueue[index].type; }

int ROB::getDest(int index) const { return ROBqueue[index].dest; }

int32_t ROB::getPC(int index) const { return ROBqueue[index].pc; }

bool ROB::getHalt(int index) const { return ROBqueue[index].halt; }

const BranchPredictorSnapshot &ROB::getRASCkpt(int index) const {
  return ROBqueue[index].ckpt.BPsnapshot;
}

const int *ROB::getRATPrfCkpt(int index) const {
  return ROBqueue[index].ckpt.RATsnapshot.RAT_snapshot;
}

uint32_t ROB::getFlHeadSeqCkpt(int index) const {
  return ROBqueue[index].ckpt.flHeadSeqCkpt;
}

void ROB::setROBCommitReady(int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].isCommitReady = true;
}

int ROB::getPredictedPC(int index) const { return ROBqueue[index].predictedPC; }

uint8_t ROB::getLsqTailSnapshot(int index) const { return ROBqueue[index].lsqTailSnapshot; }

int ROB::getNewPhy(int index) const { return ROBqueue[index].newPhy; }

int ROB::getOldPhy(int index) const { return ROBqueue[index].oldPhy; }

bool ROB::isHeadCommitReady() const { return peek().isCommitReady; }

uint64_t ROB::headSeq() const { return ROBqueue[head].seq; }

int ROB::getHead() const { return head; }

int ROB::getTail() const { return tail; }

void ROB::flush(int squashIndex) { tail = (squashIndex + 1) & 0x3F; }
