#include "../include/BRU.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cstdint>

void BRU::BRUExecute(int32_t op1, int32_t op2, int32_t pc, int32_t imm,
                     Operation op, int robIndex, uint64_t robSeq) {
  bool taken = false;
  switch (op) {
  case Operation::EQ:
    taken = (op1 == op2);
    break;
  case Operation::NE:
    taken = (op1 != op2);
    break;
  case Operation::LT:
    taken = (op1 < op2);
    break;
  case Operation::GE:
    taken = (op1 >= op2);
    break;
  case Operation::LTU:
    taken = (static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2));
    break;
  case Operation::GEU:
    taken = (static_cast<uint32_t>(op1) >= static_cast<uint32_t>(op2));
    break;
  default:
    taken = false;
    break;
  }
  push({pc, taken ? pc + imm : pc + 4, robIndex, robSeq});
}

void BRU::push(BranchResult result) {
  for (int i = 0; i < BRU_CAP; i++)
    if (!slotValid[i]) {
      outputBuffer[i] = result;
      slotValid[i] = true;
      return;
    }
}

int32_t BRU::headPCFrom() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].pcFrom : 0;
}
int32_t BRU::headPCResult() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].pcResult : 0;
}
int BRU::headRobIndex() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robIndex : -1;
}
uint64_t BRU::headRobSeq() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robSeq : 0;
}

bool BRU::isFull() const {
  for (int i = 0; i < BRU_CAP; i++) {
    if (!slotValid[i])
      return false;
  }
  return true;
}

bool BRU::isEmpty() const {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i])
      return false;
  }
  return true;
}

void BRU::remove(uint64_t robSeq) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq == robSeq) {
      slotValid[i] = false;
      return;
    }
  }
}

void BRU::flush(uint64_t seq) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq > seq)
      slotValid[i] = false;
  }
}

void BRU::tick(const BRUInput &input, systemState &CPUstate) 
{
  // BRU execute
  if (!isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    BranchReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = input.branchRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
          foundAny = true;
        } else if (input.ROBModule.getSeq(rs.robIndex) <
                   input.ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = input.ROBModule.getSeq(Execute_RS.robIndex);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash && execSeq < input.squashDetect.SquashSeq)) {
        CPUstate.BRUModule.BRUExecute(
            Execute_RS.vj, Execute_RS.vk, Execute_RS.pc, Execute_RS.imm,
            Execute_RS.op, Execute_RS.robIndex, execSeq);
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].free = true;
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].qj = -1;
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].qk = -1;
      }
    }
  }
  if (input.squashDetect.needSquash){
    // 5. clear the wrong BRU outputBuffer
    CPUstate.BRUModule.flush(input.squashDetect.SquashSeq);
  }
}