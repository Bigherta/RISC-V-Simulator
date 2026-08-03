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
  outputBuffer[tail] = result;
  tail = (tail + 1) & 0b11;
}

BranchResult BRU::pop() {
  auto temp = outputBuffer[head];
  head = (head + 1) & 0b11;
  return temp;
}

BranchResult BRU::peek() const { return outputBuffer[head]; }

bool BRU::isFull() const { return ((tail + 1) & 0b11) == head; }

bool BRU::isEmpty() const { return tail == head; }

void BRU::flush(int tag) {
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
