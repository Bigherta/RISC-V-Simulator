#include "../include/CPU.hpp"
#include <cstdint>

void BRU::BRUExecute(int32_t op1, int32_t op2, int32_t pc, int32_t imm,
                     Operation op, RobTag robTag) {
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
  push({pc, taken ? pc + imm : pc + 4, robTag});
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
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].pcFrom : 0;
}
int32_t BRU::headPCResult() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].pcResult : 0;
}
uint8_t BRU::headRobTag() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 ||
         ROB::isOlder(outputBuffer[i].robTag, outputBuffer[best].robTag)))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robTag : 0;
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

void BRU::remove(uint8_t robTag) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag == robTag) {
      slotValid[i] = false;
      return;
    }
  }
}

void BRU::flush(uint8_t tag) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && !ROB::isOlder(outputBuffer[i].robTag, tag))
      slotValid[i] = false;
  }
}

void BRU::tick(const BRUInput &input, systemState &CPUstate) {
  // stage 1: consume the dispatch bus (select was already evaluated on the
  // DispatchArbiter snapshot side; RS slot release is handled by RSUnit.tick)
  if (input.dispatch.valid) {
    auto &rs = input.RSModule.branchRS[input.dispatch.rsIndex];
    CPUstate.BRUModule.BRUExecute(input.PRFModule.getOperandValue(rs.src1),
                                  input.PRFModule.getOperandValue(rs.src2),
                                  rs.pc, rs.imm, rs.op,
                                  input.dispatch.robTag);
  }
  // BRU writeBack
  if (!isEmpty()) {
    uint8_t brRobTag = headRobTag();
    CPUstate.BRUModule.remove(brRobTag);
  }
  // clear the wrong BRU outputBuffer
  if (input.squashDetect.needSquash) {
    CPUstate.BRUModule.flush(input.squashDetect.SquashTag);
  }
}