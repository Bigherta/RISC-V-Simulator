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
  outputBuffer[tail] = result;
  tail = (tail + 1) & 0b11;
}

ExecuteResult ALU::pop() {
  auto temp = outputBuffer[head];
  head = (head + 1) & 0b11;
  return temp;
}

ExecuteResult ALU::peek() const { return outputBuffer[head]; }

bool ALU::isFull() const { return ((tail + 1) & 0b11) == head; }
bool ALU::isEmpty() const { return tail == head; }

void ALU::flush(int tag) {
  int first_flushed = -1;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0b11) {
    if (outputBuffer[cur].robTag > tag) {
      if (first_flushed == -1)
        first_flushed = cur;
    }
  }
  if (first_flushed != -1)
    tail = first_flushed;
}