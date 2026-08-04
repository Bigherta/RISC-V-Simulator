#include "../include/BRU.hpp"
#include <cstdint>

void BRU::BRUExecute(int32_t op1, int32_t op2, int32_t pc, int32_t imm,
                     Operation op, int ROBTag) {
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
  push({taken ? pc + imm : pc + 4, ROBTag});
}

void BRU::push(BranchResult result) {
  for (int i = 0; i < BRU_CAP; i++)
    if (!slotValid[i]) { outputBuffer[i] = result; slotValid[i] = true; return; }
}

BranchResult BRU::peek() const {
  int best = -1;
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && (best == -1 || outputBuffer[i].robTag < outputBuffer[best].robTag))
      best = i;
  }
  if (best >= 0)
    return outputBuffer[best];
  return {};
}

BranchResult BRU::getEntry(int index) const { return outputBuffer[index]; }

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

void BRU::remove(int robTag) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag == robTag) {
      slotValid[i] = false;
      return;
    }
  }
}

void BRU::flush(int tag) {
  for (int i = 0; i < BRU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag > tag)
      slotValid[i] = false;
  }
}