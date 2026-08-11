#include "../include/PRF.hpp"
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
  // permanently reserved -- it never enters the free list (see checkPRFInvariant).
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
