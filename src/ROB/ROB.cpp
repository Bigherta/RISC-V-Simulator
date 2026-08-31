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

bool ROB::isHaltCommitted() const { return haltCommitted; }

int ROB::getHaltRd() const { return haltRd; }

uint8_t ROB::getNextTag() const { return next_tag; }

void ROB::updateNextTag() { next_tag = (next_tag + 1) & 0x7F; }

bool ROB::isFull() const { return (head ^ next_tag) == 0x40; }

bool ROB::isEmpty() const { return head == next_tag; }

int ROB::push(ROBEntry entry) {
  entry.tag = next_tag;
  ROBqueue[next_tag & 0x3F] = entry;
  int index = next_tag & 0x3F;
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

bool ROB::isIndirect(int index) const { return ROBqueue[index].isIndirect; }

bool ROB::isHeadCommitReady() const {
  return ROBqueue[getHead() & 0x3F].isCommitReady;
}

int ROB::getHead() const { return head; }

bool ROB::isHeadHalt() const { return isHalt(getHead() & 0x3F); }

ROBType ROB::headType() const { return getType(getHead() & 0x3F); }

int ROB::headDest() const { return ROBqueue[head & 0x3F].dest; }

void ROB::flush(RobTag squashTag) {
  next_tag = (ROBqueue[squashTag & 0x3F].tag + 1) & 0x7F;
}

void ROB::tick(const ROBInput &input, systemState &CPUstate) {
  if (input.issuePacket.valid) {
    CPUstate.ROBModule.push(input.issuePacket.robEntry);
  }
  // BRU set ROB ready
  if (!input.BRUModule.isEmpty()) {
    uint8_t brRobTag = input.BRUModule.headRobTag();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(brRobTag, input.squashDetect.SquashTag))) {
      CPUstate.ROBModule.setROBCommitReady(((brRobTag) & 0x3F));
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
          CPUstate.ROBModule.setROBCommitReady(((lqTag) & 0x3F));
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
          CPUstate.ROBModule.setROBCommitReady(((sqTag) & 0x3F));
        }
      }
    }
  }
  // CDB set ROB ready
  CDBOutput cdbOut = input.cdbOut;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag)) {
      auto robIdx = ((cdbOut.result.robTag) & 0x3F);
      if (!isEmpty() && !ROB::isOlder(cdbOut.result.robTag, getHead())) {
        CPUstate.ROBModule.setROBCommitReady(robIdx);
      }
    }
  }
  // ROB squash
  if (input.squashDetect.needSquash) {
    CPUstate.ROBModule.flush(input.squashDetect.SquashTag);
    return;
  }
  if (isEmpty() || !isHeadCommitReady())
    return;
  int headIdx = (getHead() & 0x3F);
  CPUstate.ROBModule.pop();
  if (isHeadHalt()) {
    CPUstate.ROBModule.haltCommitted = true;
    CPUstate.ROBModule.haltRd = headDest();
  }
}