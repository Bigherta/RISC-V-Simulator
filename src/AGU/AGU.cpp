#include "../include/AGU.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
void AGU::push(int32_t op1, int32_t op2, Operation op, RobTag robTag,
               uint8_t lsqIndex) {
  int32_t value;
  value = op1 + op2;
  AddressCalculateResult result{value, robTag, lsqIndex};
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
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].value : 0;
}
uint8_t AGU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robTag : 0;
}
uint8_t AGU::headlsqIndex() const {
  int best = -1;
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].lsqIndex : 0;
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

void AGU::remove(uint8_t robTag) {
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag == robTag) {
      slotValid[i] = false;
      return;
    }
  }
}

void AGU::flush(uint8_t tag) {
  for (int i = 0; i < AGU_CAP; i++) {
    if (slotValid[i] && !ROB::isOlder(outputBuffer[i].robTag, tag))
      slotValid[i] = false;
  }
}
void AGU::tick(const AGUInput &input, systemState &CPUstate) {
  if (input.dispatch.valid) {
    if (input.dispatch.rsType == RSType::Load) {
      auto &rs = input.RSModule.loadRS[input.dispatch.rsIndex];
      CPUstate.AGUModule.push(rs.vj, rs.vk, rs.op,
                              input.dispatch.robTag, rs.lsqIndex);
    } else {
      auto &rs = input.RSModule.storeAddressRS[input.dispatch.rsIndex];
      CPUstate.AGUModule.push(rs.vj, rs.vk, rs.op,
                              input.dispatch.robTag, rs.lsqIndex);
    }
  }
  // AGU remove the first entry every cycle
  if (!isEmpty()) {
    CPUstate.AGUModule.remove(headRobTag());
  }
  // clear the wrong AGU buffer
  if (input.squashDetect.needSquash) {
    CPUstate.AGUModule.flush(input.squashDetect.SquashTag);
  }
}