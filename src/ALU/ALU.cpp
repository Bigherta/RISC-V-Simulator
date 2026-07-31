#include "../include/ALU.hpp"
void ALU::ALUExecute(int32_t op1, int32_t op2, Operation op, int ROBTag) {
  int32_t result;
  if (isMemoryOp(op)) {
    result = op1 + op2;
  } else {
    switch (op) {
    case Operation::ADD:
    case Operation::AUIPC:
      result = op1 + op2;
      break;
    case Operation::SUB:
      result = op1 - op2;
      break;
    case Operation::XOR:
      result = op1 ^ op2;
      break;
    case Operation::OR:
      result = op1 | op2;
      break;
    case Operation::AND:
      result = op1 & op2;
      break;
    case Operation::SL:
      result = static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
      break;
    case Operation::SRL:
      result = static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
      break;
    case Operation::SRA:
      result = op1 >> (op2 & 0x1F);
      break;
    case Operation::SLT:
      result = op1 < op2 ? 1 : 0;
      break;
    case Operation::SLTU:
      result = static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
      break;
    case Operation::LUI:
      result = op2;
      break;
    default:
      result = 0;
      break;
    }
  }
  push({result, ROBTag, isMemoryOp(op)});
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

ExecuteResult ALU::peek() const {
  return outputBuffer[head];
}

bool ALU::isFull() const { return ((tail + 1) & 0b11) == head; }
bool ALU::isEmpty() const { return tail == head; }