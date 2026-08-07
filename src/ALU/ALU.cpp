#include "../include/ALU.hpp"
#include <cstdint>
int32_t ALU::ALUCalculate(int32_t op1, int32_t op2, Operation op) {
  if (isMemoryOp(op) || isControlOp(op)) {
    return op1 + op2;
  } else {
    switch (op) {
    case Operation::ADD:
    case Operation::AUIPC:
      return op1 + op2;
      break;
    case Operation::SUB:
      return op1 - op2;
      break;
    case Operation::XOR:
      return op1 ^ op2;
      break;
    case Operation::OR:
      return op1 | op2;
      break;
    case Operation::AND:
      return op1 & op2;
      break;
    case Operation::SL:
      return static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
      break;
    case Operation::SRL:
      return static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
      break;
    case Operation::SRA:
      return op1 >> (op2 & 0x1F);
      break;
    case Operation::SLT:
      return op1 < op2 ? 1 : 0;
      break;
    case Operation::SLTU:
      return static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
      break;
    case Operation::LUI:
      return op2;
      break;
    default:
      return 0;
      break;
    }
  }
}

void ALU::push(ExecuteResult result) {
  for (int i = 0; i < ALU_CAP; i++)
    if (!slotValid[i]) { outputBuffer[i] = result; slotValid[i] = true; return; }
}

ExecuteResult ALU::peek() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] && (best == -1 || outputBuffer[i].robTag < outputBuffer[best].robTag))
      best = i;
  }
  if (best >= 0)
    return outputBuffer[best];
  return {};
}

ExecuteResult ALU::getEntry(int index) const { return outputBuffer[index]; }

bool ALU::isFull() const {
  for (int i = 0; i < ALU_CAP; i++) {
    if (!slotValid[i])
      return false;
  }
  return true;
}

bool ALU::isEmpty() const {
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i])
      return false;
  }
  return true;
}

void ALU::remove(int robTag) {
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag == robTag) {
      slotValid[i] = false;
      return;
    }
  }
}

void ALU::flush(int tag) {
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robTag > tag)
      slotValid[i] = false;
  }
}