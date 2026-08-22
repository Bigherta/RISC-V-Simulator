#include "../include/ROB.hpp"
#include "../include/CPU.hpp"
#include <cassert>
#include <cstdint>

bool ROB::isOlder(RobTag tag_a, RobTag tag_b) {
  if ((tag_a >> 6) != (tag_b >> 6))
    return ((tag_a & 63) > (tag_b & 63));
  return ((tag_a & 63) < (tag_b & 63));
}

bool ROB::isYounger(RobTag tag_a, RobTag tag_b) {
  return isOlder(tag_b, tag_a);
}

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

uint8_t ROB::getTag(int index) const { return ROBqueue[index].tag; }

bool ROB::isCommitReadyAt(int index) const {
  return ROBqueue[index].isCommitReady;
}

ROBType ROB::getType(int index) const { return ROBqueue[index].type; }

int ROB::getDest(int index) const { return ROBqueue[index].dest; }

int32_t ROB::getPC(int index) const { return ROBqueue[index].pc; }

bool ROB::isHalt(int index) const { return ROBqueue[index].halt; }

uint8_t ROB::getCkptId(int index) const { return ROBqueue[index].ckptId; }

void ROB::setROBCommitReady(int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].isCommitReady = true;
}

int ROB::getPredictedPC(int index) const { return ROBqueue[index].predictedPC; }

uint8_t ROB::getLqtTailSnapshot(int index) const {
  return ROBqueue[index].lqtTailSnapshot;
}

uint8_t ROB::getSqtTailSnapshot(int index) const {
  return ROBqueue[index].sqTailSnapshot;
}

int ROB::getNewPhy(int index) const { return ROBqueue[index].newPhy; }

int ROB::getOldPhy(int index) const { return ROBqueue[index].oldPhy; }

bool ROB::isCall(int index) const { return ROBqueue[index].isCall; }

bool ROB::isRet(int index) const { return ROBqueue[index].isRet; }

bool ROB::isHeadCommitReady() const {
  return ROBqueue[idx(getHead())].isCommitReady;
}

int ROB::getHead() const { return head; }

bool ROB::isHeadHalt() const { return isHalt(idx(getHead())); }

ROBType ROB::headType() const { return getType(idx(getHead())); }

int ROB::headDest() const { return ROBqueue[idx(head)].dest; }

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
  // LQ set ROB ready (loads) - commit guard: a READY load must not cross an
  // older store with unresolved address
  auto lqHead = input.LQModule.getHead();
  for (int k = 0; k < MEMQ_SCAN_WINDOW; ++k) {
    uint8_t i = (lqHead + k) & 0x0F;
    if (!input.LQModule.isActive(i))
      continue;
    if (input.LQModule.isReadyToCommit(i) &&
        !input.SQModule.hasOlderUnresolvedAddressStore(
            input.LQModule.getRobTag(i))) {
      auto lqTag = input.LQModule.getRobTag(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(lqTag, input.squashDetect.SquashTag))) {
        if (!isEmpty() && !ROB::isOlder(lqTag, getHead())) {
          CPUstate.ROBModule.setROBCommitReady(getIndexByTag(lqTag));
        }
      }
    }
  }
  // SQ set ROB ready (stores)
  auto sqHead = input.SQModule.getHead();
  for (int k = 0; k < MEMQ_SCAN_WINDOW; ++k) {
    uint8_t i = (sqHead + k) & 0x0F;
    if (!input.SQModule.isActive(i))
      continue;
    if (input.SQModule.isReadyToCommit(i)) {
      auto sqTag = input.SQModule.getRobTag(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(sqTag, input.squashDetect.SquashTag))) {
        if (!isEmpty() && !ROB::isOlder(sqTag, getHead())) {
          CPUstate.ROBModule.setROBCommitReady(getIndexByTag(sqTag));
        }
      }
    }
  }
  // CDB set ROB ready
  CDBOutput cdbOut = input.cdbOut;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag)) {
      auto robIdx = getIndexByTag(cdbOut.result.robTag);
      if (!isEmpty() && !ROB::isOlder(cdbOut.result.robTag, getHead())) {
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
  CPUstate.ROBModule.pop();
  if (isHeadHalt()) {
    CPUstate.ROBModule.haltCommitted = true;
    CPUstate.ROBModule.haltRd = headDest();
  }
}