#include "../include/AGU.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cstdint>
void AGU::push(int32_t op1, int32_t op2, Operation op, int robIndex,
               uint64_t robSeq) {
  int32_t value;
  value = op1 + op2;
  AddressCalculateResult result{value, robIndex, robSeq};
  for (int i = 0; i < AGU_CAP; i++)
    if (!slotValid[i]) {
      outputBuffer[i] = result;
      slotValid[i] = true;
      return;
    }
}

int32_t AGU::headValue() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].value : 0;
}
int AGU::headRobIndex() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robIndex : -1;
}
uint64_t AGU::headRobSeq() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robSeq : 0;
}
bool AGU::isFull() const {
  for (int i = 0; i < AGU_CAP; i++) {
    if (!slotValid[i])
      return false;
  }
  return true;
}

bool AGU::isEmpty() const {
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i])
      return false;
  }
  return true;
}

void AGU::remove(uint64_t robSeq) {
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq == robSeq) {
      slotValid[i] = false;
      return;
    }
  }
}

void AGU::flush(uint64_t seq) {
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq > seq)
      slotValid[i] = false;
  }
}
void AGU::tick(const AGUInput &input, systemState &CPUstate) {
  if (input.dispatch.valid) {
    if (input.dispatch.rsType == RSType::Load) {
      auto &rs = input.RSModule.loadRS[input.dispatch.rsIndex];
      CPUstate.AGUModule.push(rs.vj, rs.vk, rs.op, rs.robIndex,
                              input.dispatch.robSeq);
    } else {
      auto &rs = input.RSModule.storeAddressRS[input.dispatch.rsIndex];
      CPUstate.AGUModule.push(rs.vj, rs.vk, rs.op, rs.robIndex,
                              input.dispatch.robSeq);
    }
  }
  // AGU remove the first entry every cycle
  if (!isEmpty()) {
    CPUstate.AGUModule.remove(headRobSeq());
  }
  // clear the wrong AGU buffer
  if (input.squashDetect.needSquash) {
    CPUstate.AGUModule.flush(input.squashDetect.SquashSeq);
  }
}