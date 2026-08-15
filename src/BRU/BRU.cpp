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

void BRU::tick(const BRUInput &input, systemState &CPUstate) {
  // 段1：消费派发总线（select 已在 DispatchArbiter 快照边求值；
  // RS 槽位释放由 RSUnit.tick 自理）
  if (input.dispatch.valid) {
    auto &rs = input.RSModule.branchRS[input.dispatch.rsIndex];
    CPUstate.BRUModule.BRUExecute(rs.vj, rs.vk, rs.pc, rs.imm, rs.op,
                                  rs.robIndex, input.dispatch.robSeq);
  }
  // BRU writeBack
  SquashInfo BranchSquash;
  if (!isEmpty()) {
    int index = headRobIndex();
    uint64_t brRobSeq = headRobSeq();
    int pcResult = headPCResult();
    int pcFrom = headPCFrom();
    if (index >= 0 &&
        (!input.squashDetect.needSquash ||
         (input.squashDetect.needSquash && brRobSeq < input.squashDetect.SquashSeq))) {
      auto actualPC = pcResult;
      if (actualPC != input.ROBModule.getPredictedPC(index)) {
        if (debug::enabled(debug::TOPIC_BRANCH))
          debug::print("squash seq=%llu pc=%u (from %u)\n",
                       static_cast<unsigned long long>(brRobSeq), actualPC,
                       pcFrom);
        BranchSquash.needSquash = true;
        BranchSquash.SquashPC = actualPC;
        BranchSquash.SquashIndex = index;
        BranchSquash.SquashSeq = brRobSeq;
      }
    }
    CPUstate.BRUModule.remove(brRobSeq);
  }
  if (BranchSquash.needSquash)
    CPUstate.flushArbiter.receive(BranchSquash);
  // clear the wrong BRU outputBuffer
  if (input.squashDetect.needSquash) {
    CPUstate.BRUModule.flush(input.squashDetect.SquashSeq);
  }
}