#include "../include/PRF.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstring>
#include <stdexcept>

PRF::PRF() {
  for (int i = 0; i < PRF_CAP; ++i) {
    PhysicalRegs[i].value = 0;
    PhysicalRegs[i].ready = false;
  }
  // P1-P31 are bound to x1-x31 at reset (RAT[x] = Px, committed init value 0).
  // x0 is never renamed (all issue paths skip rd==0): RAT[0] stays InvalidPhy(=0)
  // and P0 is permanently reserved -- it never enters the free list, which
  // makes InvalidPhy=0 a valid "no register" sentinel everywhere.
  for (int i = 0; i < REGISTER_CAP; ++i)
    PhysicalRegs[i].ready = true;
  // P32-P127 enter the free list; empty tail slots stay InvalidPhy (=0)
  memset(freeList, 0, sizeof(freeList));
  for (int i = REGISTER_CAP; i < PRF_CAP; ++i)
    freeList[(tailSeq++) & (PRF_CAP - 1)] = i;
}

uint8_t PRF::pop() {
  if (isFreeListEmpty())
    throw std::runtime_error("PRF free list underflow!");
  uint8_t phy = freeList[headSeq & (PRF_CAP - 1)];
  assert(phy != InvalidPhy); // P0-dead invariant: popped tags are real registers
  headSeq++;
  PhysicalRegs[phy].ready = false; // prevent reading the stale data
  return phy;
}

void PRF::push(int index) {
  assert(index != InvalidPhy); // P0-dead invariant: only real tags are recycled
  freeList[tailSeq & (PRF_CAP - 1)] = index;
  tailSeq++;
}

bool PRF::isFreeListEmpty() const { return headSeq == tailSeq; }

uint32_t PRF::getHeadSeq() const { return headSeq; }

void PRF::restoreHead(uint32_t ckptHeadSeq) {
  assert(tailSeq - ckptHeadSeq <= PRF_CAP);
  headSeq = ckptHeadSeq;
}

bool PRF::isReady(int index) const { return PhysicalRegs[index].ready; }
int32_t PRF::getValue(int index) const { return PhysicalRegs[index].value; }
void PRF::write(int index, int32_t value) {
  PhysicalRegs[index].ready = true;
  PhysicalRegs[index].value = value;
}

void PRF::tick(const PRFInput &input, systemState &CPUstate) {
  // LQ load value write-back - same commit guard as ROB: a load that crossed
  // an unresolved-address older store must not write its value into the PRF
  auto head = input.LQModule.getHead();
  for (int k = 0; k < MEMQ_SCAN_WINDOW; ++k) {
    uint8_t i = (head + k) & 0x0F;
    if (!input.LQModule.isActive(i))
      continue;
    if (input.LQModule.isReadyToCommit(i) &&
        !input.SQModule.hasOlderUnresolvedAddressStore(
            input.LQModule.getRobTag(i))) {
      auto lqTag = input.LQModule.getRobTag(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(lqTag, input.squashDetect.SquashTag))) {
        if (!input.ROBModule.isEmpty() &&
            !ROB::isOlder(lqTag, input.ROBModule.getHead())) {
          int newPhy =
              input.ROBModule.getNewPhy(input.ROBModule.getIndexByTag(lqTag));
          if (newPhy != InvalidPhy) {
            CPUstate.PRFModule.write(newPhy, input.LQModule.getValue(i));
            if (debug::enabled(debug::TOPIC_PRF))
              debug::print("PRF write P%d = %d (lq)\n", newPhy,
                           input.LQModule.getValue(i));
          }
        }
      }
    }
  }
  CDBOutput cdbOut = input.cdbOut;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag)) {
      auto robIdx = input.ROBModule.getIndexByTag(cdbOut.result.robTag);
      auto isControl = cdbOut.result.isControl;
      if (!isControl) {
        auto value = cdbOut.result.value;
        int newPhy = input.ROBModule.getNewPhy(robIdx);
        if (newPhy != InvalidPhy) {
          CPUstate.PRFModule.write(newPhy, value);
          if (debug::enabled(debug::TOPIC_PRF)) {
            debug::print("PRF write P%d = %d (cdb)\n", newPhy, value);
            if (isReady(newPhy) && getValue(newPhy) != value)
              debug::print("PRF mismatch P%d: rob=%d prf=%d\n", newPhy, value,
                           getValue(newPhy));
          }
        }
      }
    }
  }
  if (input.issuePacket.valid) {
    CPUstate.PRFModule.PRFHeadCkpt[input.issuePacket.robEntry.ckptId] =
        headSeq + (input.issuePacket.allocDest ? 1 : 0);
    if (input.issuePacket.allocDest) {
      auto headphy = CPUstate.PRFModule.pop();
      assert(headphy == input.issuePacket.phy);
      if (input.issuePacket.isControl) {
        CPUstate.PRFModule.write(input.issuePacket.phy,
                                 input.issuePacket.pc + 4);
        if (debug::enabled(debug::TOPIC_PRF))
          debug::print("PRF link P%d = %d (pc+4)\n", input.issuePacket.phy,
                       input.issuePacket.pc + 4);
      }
    }
  }
  if (input.squashDetect.needSquash) {
    auto index = input.squashDetect.SquashIndex;
    if (index >= 0) {
      auto ckptHead = PRFHeadCkpt[input.squashDetect.CkptId];
      CPUstate.PRFModule.restoreHead(ckptHead);
    }
    return;
  }
  if (input.ROBModule.isEmpty() || !input.ROBModule.isHeadCommitReady())
    return;
  int headIdx = ROB::idx(input.ROBModule.getHead());
  if (!input.ROBModule.isHeadHalt() &&
      (input.ROBModule.headType() == ROBType::REGISTER ||
       input.ROBModule.headType() == ROBType::LINK)) {
    int newPhy = input.ROBModule.getNewPhy(headIdx);
    int oldPhy = input.ROBModule.getOldPhy(headIdx);
    if (oldPhy != InvalidPhy)
      CPUstate.PRFModule.push(oldPhy);
  }
}
