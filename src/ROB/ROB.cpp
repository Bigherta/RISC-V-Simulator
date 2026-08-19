#include "../include/ROB.hpp"
#include "../include/CPU.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>

bool ROB::isOlder(RobTag tag_a, RobTag tag_b) {
  if ((tag_a >> 6) != (tag_b >> 6))
    return ((tag_a & 63) > (tag_b & 63));
  return ((tag_a & 63) < (tag_b & 63));
}

bool ROB::isYounger(RobTag tag_a, RobTag tag_b) { return isOlder(tag_b, tag_a); }

int ROB::idx(RobTag t) { return t & 0x3F; }

int ROB::getIndexByTag(RobTag tag) const {
  assert(ROBqueue[idx(tag)].tag == tag);
  return idx(tag);
}

bool ROB::isHaltCommitted() const { return haltCommitted; }

int ROB::getHaltRd() const { return haltRd; }

uint8_t ROB::getNextTag() const { return next_tag; }

void ROB::updateNextTag() { next_tag = (next_tag + 1) & 0x7F; }

bool ROB::isFull() const { return (head ^ next_tag) == 0x40; }

bool ROB::isEmpty() const { return head == next_tag; }

int ROB::push(ROBEntry entry) {
  entry.tag = next_tag;
  ROBqueue[idx(next_tag)] = entry;
  int index = idx(next_tag);
  updateNextTag();
  return index;
}

void ROB::pop() { head = (head + 1) & 0x7F; }

ROBEntry ROB::peek() const {
  if (isEmpty())
    throw std::runtime_error("peek an empty ROB!");
  return ROBqueue[idx(head)];
}

uint8_t ROB::getTag(int index) const { return ROBqueue[index].tag; }

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

uint8_t ROB::getLsqTailSnapshot(int index) const {
  return ROBqueue[index].lsqTailSnapshot;
}

int ROB::getNewPhy(int index) const { return ROBqueue[index].newPhy; }

int ROB::getOldPhy(int index) const { return ROBqueue[index].oldPhy; }

bool ROB::getIsCall(int index) const { return ROBqueue[index].isCall; }

bool ROB::getIsRet(int index) const { return ROBqueue[index].isRet; }

bool ROB::isHeadCommitReady() const { return peek().isCommitReady; }

uint8_t ROB::headTag() const { return head; }

int ROB::getHead() const { return head; }

void ROB::flush(int squashIndex) {
  next_tag = (ROBqueue[squashIndex].tag + 1) & 0x7F;
}

void ROB::tick(const ROBInput &input, systemState &CPUstate) {
  // issue apply: push the pre-built ROB entry (robIndex >= 0 excludes the
  // RV_INVALID "pop-only" packet, whose robIndex stays at the default -1)
  if (input.issuePacket.valid && input.issuePacket.robIndex >= 0) {
    int idx = CPUstate.ROBModule.push(input.issuePacket.robEntry);
    assert(idx == input.issuePacket.robIndex);
  }
  // BRU set ROB ready
  if (!input.BRUModule.isEmpty()) {
    uint8_t brRobTag = input.BRUModule.headRobTag();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(brRobTag, input.squashDetect.SquashTag))) {
      CPUstate.ROBModule.setROBCommitReady(getIndexByTag(brRobTag));
    }
  }
  // LSQ set ROB ready
  auto head = input.LSQModule.getHead();
  for (int k = 0; k < (LSQ_CAP >> 3); ++k) {
    uint8_t i = (head + k) & 0x3F;
    if (!input.LSQModule.isActive(i))
      continue;
    if (input.LSQModule.isReadyToCommit(i)) {
      auto lsqTag = input.LSQModule.getRobTag(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(lsqTag,
                         input.squashDetect.SquashTag))) {
        if (!isEmpty() && !ROB::isOlder(lsqTag, headTag())) {
          CPUstate.ROBModule.setROBCommitReady(getIndexByTag(lsqTag));
        }
      }
    }
  }
  // CDB set ROB ready
  CDBOutput cdbOut = input.cdbOut;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        ROB::isOlder(cdbOut.result.robTag,
                         input.squashDetect.SquashTag)) {
      auto robIdx = getIndexByTag(cdbOut.result.robTag);
      if (!isEmpty() &&
          !ROB::isOlder(cdbOut.result.robTag, headTag())) {
        CPUstate.ROBModule.setROBCommitReady(robIdx);
      }
    }
  }
  // ROB squash
  if (input.squashDetect.needSquash) {
    CPUstate.ROBModule.flush(input.squashDetect.SquashIndex);
    return;
  }
  if (isEmpty() || !isHeadCommitReady())
    return;
  int headIdx = idx(getHead());
  auto rob_entry = peek();
  CPUstate.ROBModule.pop();
  if (rob_entry.halt) {
    CPUstate.ROBModule.haltCommitted = true;
    CPUstate.ROBModule.haltRd = rob_entry.dest;
  }
}