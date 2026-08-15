#include "../include/ROB.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cstdint>
#include <stdexcept>

bool ROB::isFull() const { return ((tail + 1) & 0x3F) == head; }

bool ROB::isEmpty() const { return head == tail; }

int ROB::push(ROBEntry entry) {
  entry.seq = next_seq++;
  ROBqueue[tail] = entry;
  int index = tail;
  tail = (tail + 1) & 0x3F;
  return index;
}

void ROB::pop() { head = (head + 1) & 0x3F; }

ROBEntry ROB::peek() const {
  if (isEmpty())
    throw std::runtime_error("peek an empty ROB!");
  return ROBqueue[head];
}

uint64_t ROB::getSeq(int index) const { return ROBqueue[index].seq; }

bool ROB::isCommitReadyAt(int index) const {
  return ROBqueue[index].isCommitReady;
}

ROBType ROB::getType(int index) const { return ROBqueue[index].type; }

int ROB::getDest(int index) const { return ROBqueue[index].dest; }

int32_t ROB::getPC(int index) const { return ROBqueue[index].pc; }

bool ROB::getHalt(int index) const { return ROBqueue[index].halt; }

const BranchPredictorSnapshot &ROB::getRASCkpt(int index) const {
  return ROBqueue[index].ckpt.BPsnapshot;
}

const int *ROB::getRATPrfCkpt(int index) const {
  return ROBqueue[index].ckpt.RATsnapshot.RAT_snapshot;
}

uint32_t ROB::getFlHeadSeqCkpt(int index) const {
  return ROBqueue[index].ckpt.flHeadSeqCkpt;
}

void ROB::setROBCommitReady(int index) {
  if (index < 0 || index >= ROB_CAP)
    return;
  ROBqueue[index].isCommitReady = true;
}

int ROB::getPredictedPC(int index) const { return ROBqueue[index].predictedPC; }

uint8_t ROB::getLsqTailSnapshot(int index) const {
  return ROBqueue[index].lsqTailSnapshot;
}

int ROB::getNewPhy(int index) const { return ROBqueue[index].newPhy; }

int ROB::getOldPhy(int index) const { return ROBqueue[index].oldPhy; }

bool ROB::isHeadCommitReady() const { return peek().isCommitReady; }

uint64_t ROB::headSeq() const { return ROBqueue[head].seq; }

int ROB::getHead() const { return head; }

int ROB::getTail() const { return tail; }

void ROB::flush(int squashIndex) { tail = (squashIndex + 1) & 0x3F; }

void ROB::tick(const ROBInput &input, systemState &CPUstate) {
  // 1. BRU set ROB ready
  if (!input.BRUModule.isEmpty()) {
    int index = input.BRUModule.headRobIndex();
    uint64_t brRobSeq = input.BRUModule.headRobSeq();
    if (index >= 0 && (!input.squashDetect.needSquash ||
                       (input.squashDetect.needSquash &&
                        brRobSeq < input.squashDetect.SquashSeq))) {
      CPUstate.ROBModule.setROBCommitReady(index);
    }
  }
  // 2. LSQ set ROB ready
  auto head = input.LSQModule.getHead();
  auto tail = input.LSQModule.getTail();
  for (int i = head; i != ((head + (LSQ_CAP >> 3)) & 0x3F);
       i = (i + 1) & 0x3F) {
    if (i == tail)
      break;
    if (input.LSQModule.isReadyToCommit(i)) {
      auto lsqRobIndex = input.LSQModule.getRobIndex(i);
      auto lsqSeq = input.LSQModule.getRobSeq(i);
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           lsqSeq < input.squashDetect.SquashSeq)) {
        if (!isEmpty() && lsqSeq >= headSeq()) {
          CPUstate.ROBModule.setROBCommitReady(lsqRobIndex);
        }
      }
    }
  }
  // 3. CDB set ROB ready
  CDBOutput cdbOut = input.cdbArbiter;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        cdbOut.result.robSeq < input.squashDetect.SquashSeq) {
      auto robIndex = cdbOut.result.robIndex;
      auto robSeq = cdbOut.result.robSeq;
      auto isControl = cdbOut.result.isControl;
      SquashInfo JumpSquash;
      if (!isEmpty() && robSeq >= headSeq()) {
        CPUstate.ROBModule.setROBCommitReady(robIndex);
        if (isControl) {
          const auto pc = static_cast<uint32_t>(cdbOut.result.value);
          if (pc != getPredictedPC(robIndex)) {
            if (debug::enabled(debug::TOPIC_BRANCH))
              debug::print("squash seq=%llu pc=%u (jalr)\n",
                           static_cast<unsigned long long>(robSeq), pc);
            JumpSquash.needSquash = true;
            JumpSquash.SquashPC = pc;
            JumpSquash.SquashIndex = robIndex;
            JumpSquash.SquashSeq = robSeq;
          }
        }
      }
      if (JumpSquash.needSquash)
        CPUstate.flushArbiter.receive(JumpSquash);
    }
  }
  if (input.squashDetect.needSquash) {
    CPUstate.ROBModule.flush(input.squashDetect.SquashIndex);
    return;
  }
  if (isEmpty() || !isHeadCommitReady())
    return;
  int headIdx = getHead();
  auto rob_entry = peek();
  CPUstate.ROBModule.pop();
  if (rob_entry.halt) {
    CPUstate.haltCommitted = true;
    CPUstate.haltRd = rob_entry.dest;
  }
}