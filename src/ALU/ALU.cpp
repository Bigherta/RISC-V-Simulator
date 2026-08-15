#include "../include/ALU.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
void ALU::push(int32_t op1, int32_t op2, Operation op, int robIndex,
               uint64_t robSeq, bool isControl) {
  int32_t value;
  if (isControlOp(op)) {
    value = op1 + op2;
  } else {
    switch (op) {
    case Operation::ADD:
    case Operation::AUIPC:
      value = op1 + op2;
      break;
    case Operation::SUB:
      value = op1 - op2;
      break;
    case Operation::XOR:
      value = op1 ^ op2;
      break;
    case Operation::OR:
      value = op1 | op2;
      break;
    case Operation::AND:
      value = op1 & op2;
      break;
    case Operation::SL:
      value = static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
      break;
    case Operation::SRL:
      value = static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
      break;
    case Operation::SRA:
      value = op1 >> (op2 & 0x1F);
      break;
    case Operation::SLT:
      value = op1 < op2 ? 1 : 0;
      break;
    case Operation::SLTU:
      value = static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
      break;
    case Operation::LUI:
      value = op2;
      break;
    default:
      value = 0;
      break;
    }
  }
  ArithmeticCalculateResult result{value, robIndex, robSeq, isControl};
  for (int i = 0; i < ALU_CAP; i++)
    if (!slotValid[i]) {
      outputBuffer[i] = result;
      slotValid[i] = true;
      return;
    }
}

int32_t ALU::headValue() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].value : 0;
}
int ALU::headRobIndex() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robIndex : -1;
}
uint64_t ALU::headRobSeq() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].robSeq : 0;
}
bool ALU::headIsControl() const {
  int best = -1;
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] &&
        (best == -1 || outputBuffer[i].robSeq < outputBuffer[best].robSeq))
      best = i;
  }
  return best >= 0 ? outputBuffer[best].isControl : false;
}

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

void ALU::remove(uint64_t robSeq) {
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq == robSeq) {
      slotValid[i] = false;
      return;
    }
  }
}

void ALU::flush(uint64_t seq) {
  for (int i = 0; i < ALU_CAP; i++) {
    if (slotValid[i] && outputBuffer[i].robSeq > seq)
      slotValid[i] = false;
  }
}
void ALU::tick(const ALUInput &input, systemState &CPUstate) {
  // ALU execute
  if (!isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    ReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = input.IntegerRSModule.IntegerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          foundAny = true;
        } else if (input.ROBModule.getSeq(rs.robIndex) <
                   input.ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = input.ROBModule.getSeq(Execute_RS.robIndex);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           execSeq < input.squashDetect.SquashSeq)) {
        CPUstate.ALUModule.push(Execute_RS.vj, Execute_RS.vk, Execute_RS.op,
                                Execute_RS.robIndex, execSeq,
                                isControlOp(Execute_RS.op));
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].free = true;
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].qj = -1;
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].qk = -1;
      }
    }
  }
  // ALU writeBack: consume this unit's own grant on the CDB result.
  if (input.cdbArbiter.valid && input.cdbArbiter.aluGranted) {
    CPUstate.ALUModule.remove(input.cdbArbiter.result.robSeq);
  }
  // clear the wrong ALU outputBuffer
  if (input.squashDetect.needSquash) {
    CPUstate.ALUModule.flush(input.squashDetect.SquashSeq);
  }
}