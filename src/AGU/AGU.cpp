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
  // AGU execute
  if (!isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    ReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = input.LoadRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
          foundAny = true;
        } else if (input.ROBModule.getSeq(rs.robIndex) <
                   input.ROBModule.getSeq(Execute_RS.robIndex)){
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = input.StoreAddressRS[i];
      if (!rs.free && rs.qj == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
          foundAny = true;
        } else if (input.ROBModule.getSeq(rs.robIndex) <
                   input.ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = input.ROBModule.getSeq(Execute_RS.robIndex);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           execSeq < input.squashDetect.SquashSeq)) {
        CPUstate.AGUModule.push(Execute_RS.vj, Execute_RS.vk, Execute_RS.op,
                                Execute_RS.robIndex, execSeq);
        if (Execute_RS_type == 1) {
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].free = true;
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].qj = -1;
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].qk = -1;
        } else {
          CPUstate.StoreAddressRSModule.StoreAddressRS[Execute_RS_index].free =
              true;
          CPUstate.StoreAddressRSModule.StoreAddressRS[Execute_RS_index].qj =
              -1;
        }
      }
    }
  }
  // AGU writeBack: address result -> LSQ directly
  if (!isEmpty()) {
    auto aguRobSeq = headRobSeq();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         aguRobSeq < input.squashDetect.SquashSeq)) {
      auto index = input.LSQModule.getIndexBySeq(aguRobSeq);
      if (index >= 0) {
        auto value = headValue();
        auto plan = input.LSQModule.planAddressForward(index, value);
        CPUstate.LSQModule.writeAddress(static_cast<uint32_t>(value), index);
        CPUstate.LSQModule.applyStoreToLoadForward(plan);
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("AGU addr seq=%llu -> LSQ[%d] = %u\n",
                       static_cast<unsigned long long>(aguRobSeq), index,
                       static_cast<uint32_t>(value));
      }
    }
    CPUstate.AGUModule.remove(aguRobSeq);
  }
  if (input.squashDetect.needSquash) {

    CPUstate.AGUModule.flush(input.squashDetect.SquashSeq);
  }
}