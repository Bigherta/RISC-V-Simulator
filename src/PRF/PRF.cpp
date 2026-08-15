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
  // x0 is never renamed (all issue paths skip rd==0): RAT[0] stays -1 and P0 is
  // permanently reserved -- it never enters the free list (see
  // checkPRFInvariant).
  for (int i = 0; i < REGISTER_CAP; ++i)
    PhysicalRegs[i].ready = true;
  // P32-P127 enter the free list
  memset(freeList, 0xFF, sizeof(freeList));
  for (int i = REGISTER_CAP; i < PRF_CAP; ++i)
    freeList[(tailSeq++) & (PRF_CAP - 1)] = i;
}

uint8_t PRF::pop() {
  if (isFreeListEmpty())
    throw std::runtime_error("PRF free list underflow!");
  uint8_t phy = freeList[headSeq & (PRF_CAP - 1)];
  headSeq++;
  PhysicalRegs[phy].ready = false; // prevent reading the stale data
  return phy;
}

void PRF::push(int index) {
  freeList[tailSeq & (PRF_CAP - 1)] = index;
  tailSeq++;
}

bool PRF::isFreeListEmpty() const { return headSeq == tailSeq; }

uint32_t PRF::getHeadSeq() const { return headSeq; }
uint32_t PRF::getTailSeq() const { return tailSeq; }

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
  auto head = input.LSQModule.getHead();
  auto tail = input.LSQModule.getTail();
  for (int i = head; i != ((head + (LSQ_CAP >> 3)) & 0x3F);
       i = (i + 1) & 0x3F) {
    if (i == tail)
      break;
    if (input.LSQModule.isReadyToCommit(i)) {
      auto lsqRobIndex = input.LSQModule.getRobIndex(i);
      auto lsqSeq = input.LSQModule.getRobSeq(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           lsqSeq < input.squashDetect.SquashSeq)) {
        if (!input.ROBModule.isEmpty() && lsqSeq >= input.ROBModule.headSeq()) {
          int newPhy = input.ROBModule.getNewPhy(lsqRobIndex);
          if (newPhy >= 0) {
            CPUstate.PRFModule.write(newPhy, input.LSQModule.getValue(i));
            if (debug::enabled(debug::TOPIC_PRF))
              debug::print("PRF write P%d = %d (lsq)\n", newPhy,
                           input.LSQModule.getValue(i));
          }
        }
      }
    }
  }
  CDBOutput cdbOut = input.cdbArbiter;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        cdbOut.result.robSeq < input.squashDetect.SquashSeq) {
      auto robIndex = cdbOut.result.robIndex;
      auto isControl = cdbOut.result.isControl;
      if (!isControl) {
        auto value = cdbOut.result.value;
        int newPhy = input.ROBModule.getNewPhy(robIndex);
        if (newPhy >= 0) {
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
  if (input.squashDetect.needSquash) {
    return;
  }
  if (input.ROBModule.isEmpty() || !input.ROBModule.isHeadCommitReady())
    return;
  int headIdx = input.ROBModule.getHead();
  auto rob_entry = input.ROBModule.peek();
  if (rob_entry.halt) {
  } else if (rob_entry.type == ROBType::REGISTER ||
             rob_entry.type == ROBType::LINK) {
    int newPhy = input.ROBModule.getNewPhy(headIdx);
    int oldPhy = input.ROBModule.getOldPhy(headIdx);
    if (oldPhy >= 0)
      CPUstate.PRFModule.push(oldPhy);
  }
}
