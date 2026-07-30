#include "../include/ALU.hpp"
void ALU::ALUExecute(int32_t op1, int32_t op2, Op op, int ROBTag) {
  int32_t result;
  switch (op) {
  case ADD:
    result = op1 + op2;
    break;
  case SUB:
    result = op1 - op2;
    break;
  case XOR:
    result = op1 ^ op2;
    break;
  case OR:
    result = op1 | op2;
    break;
  case AND:
    result = op1 & op2;
    break;
  case SL:
    result = static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
    break;
  case SRL:
    result = static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
    break;
  case SRA:
    result = op1 >> (op2 & 0x1F);
    break;
  case SLT:
    result = op1 < op2 ? 1 : 0;
    break;
  case SLTU:
    result = static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
    break;
  default:
    result = 0;
    break;
  }
  push({result, ROBTag});
}

void ALU::push(ExecuteResult result) {
  tail = (tail + 1) & 0b11;
  outputBuffer[tail] = result;
}

ExecuteResult ALU::pop() {
  auto temp = outputBuffer[(head + 1) & 0b11];
  head = (head + 1) & 0b11;
  return temp;
}

bool ALU::isFull() { return ((tail + 1) & 0b11) == head; }
bool ALU::isEmpty() { return tail == head; }