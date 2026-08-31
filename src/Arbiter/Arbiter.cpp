#include "../include/Arbiter.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

FlushArbiter::FlushArbiter() { std::memset(this, 0, sizeof(*this)); }

void FlushArbiter::receive(SquashInfo request) {
  int w = 0;
  for (int r = 0; r < FLUSHARBITER_CAP; ++r) {
    if (requests[r].valid) {
      if (r != w) {
        requests[w] = requests[r];
        requests[r].valid = false;
      }
      ++w;
    }
  }
  if (w == FLUSHARBITER_CAP)
    throw std::runtime_error("flush arbiter overload!");
  int pos = 0;
  while (pos < w &&
         ROB::isOlder(requests[pos].requestArgs.SquashTag, request.SquashTag))
    ++pos;
  for (int i = FLUSHARBITER_CAP - 1; i > pos; --i)
    requests[i] = requests[i - 1];
  requests[pos].valid = true;
  requests[pos].requestArgs = request;
}

SquashInfo FlushArbiter::arbitResult() const {
  SquashInfo result{};
  bool found = false;
  for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
    if (requests[i].valid) {
      if (!found ||
          ROB::isOlder(requests[i].requestArgs.SquashTag, result.SquashTag)) {
        result = requests[i].requestArgs;
        found = true;
      }
    }
  }
  return result;
}

void FlushArbiter::clear(uint8_t tag) {
  for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
    if (requests[i].valid) {
      if (!ROB::isOlder(requests[i].requestArgs.SquashTag, tag)) {
        requests[i].valid = false;
      }
    }
  }
}

FlushRequest FlushArbiter::getRequest(int i) const { return requests[i]; }

void FlushArbiter::tick(const FlushArbiterInput &input, systemState &CPUstate) {
  if (input.squashDetect.needSquash)
    CPUstate.flushArbiter.clear(input.squashDetect.SquashTag);

  if (!input.BRUModule.isEmpty()) {
    SquashInfo BranchSquash;
    uint8_t brRobTag = input.BRUModule.headRobTag();
    int pcResult = input.BRUModule.headPCResult();
    int pcFrom = input.BRUModule.headPCFrom();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(brRobTag, input.squashDetect.SquashTag))) {
      auto actualPC = pcResult;
      if (actualPC != input.ROBModule.getPredictedPC(
                          ((brRobTag) & 0x3F))) {
        if (debug::enabled(debug::TOPIC_BRANCH))
          debug::print("squash tag=%u pc=%u (from %u)\n", brRobTag, actualPC,
                       pcFrom);
        BranchSquash.needSquash = true;
        BranchSquash.SquashPC = actualPC;
        BranchSquash.SquashTag = brRobTag;
        BranchSquash.CkptId =
            input.ROBModule.getCkptId(BranchSquash.SquashTag & 0x3F);
      }
    }
    if (BranchSquash.needSquash)
      CPUstate.flushArbiter.receive(BranchSquash);
  }

  CDBOutput cdbOut = input.cdbOut;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag)) {
      auto isControl = cdbOut.result.isControl;

      if (!input.ROBModule.isEmpty() &&
          !ROB::isOlder(cdbOut.result.robTag, input.ROBModule.getHead()) &&
          isControl) {
        SquashInfo JumpSquash;
        const auto pc = static_cast<uint32_t>(cdbOut.result.value);
        if (pc != input.ROBModule.getPredictedPC(
                      ((cdbOut.result.robTag) & 0x3F))) {
          if (debug::enabled(debug::TOPIC_BRANCH))
            debug::print("squash tag=%u pc=%u (jalr)\n", cdbOut.result.robTag,
                         pc);
          JumpSquash.needSquash = true;
          JumpSquash.SquashPC = pc;
          JumpSquash.SquashTag = cdbOut.result.robTag;
          JumpSquash.CkptId = input.ROBModule.getCkptId(JumpSquash.SquashTag & 0x3F);
        }
        if (JumpSquash.needSquash)
          CPUstate.flushArbiter.receive(JumpSquash);
      }
    }
  }
  

  if (!input.AGUModule.isEmpty() && isStoreMem(input.AGUModule.headMemIndex())) {
    auto aguRobTag = input.AGUModule.headRobTag();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(aguRobTag, input.squashDetect.SquashTag))) {
      auto storeAddr = static_cast<uint32_t>(input.AGUModule.headValue());
      auto lqHead = input.LQModule.getHead();
      bool violationHandled = false;
      for (int k = 0; k < LQ_CAP; ++k) {
        uint8_t i = (lqHead + k) & 0x0F;
        if (violationHandled)
          continue;  
        if (!input.LQModule.isActive(i))
          continue;
        if (input.LQModule.isAddressReady(i) &&
            input.LQModule.getAddress(i) == storeAddr &&
            (input.LQModule.getValueState(i) == ValueState::READY ||
             input.LQModule.getValueState(i) == ValueState::FETCHING) &&
            ROB::isYounger(input.LQModule.getRobTag(i), aguRobTag)) {
          auto violTag = input.LQModule.getRobTag(i);
          if ((!input.squashDetect.needSquash ||
               ROB::isOlder(violTag, input.squashDetect.SquashTag)) &&
              !input.ROBModule.isEmpty() &&
              !ROB::isOlder(violTag, input.ROBModule.getHead())) {
            SquashInfo viol;
            viol.needSquash = true;
            viol.SquashTag = violTag;
            viol.SquashPC = input.ROBModule.getPC(viol.SquashTag & 0x3F);
            viol.CkptId = input.ROBModule.getCkptId(viol.SquashTag & 0x3F);
            CPUstate.flushArbiter.receive(viol);
            violationHandled = true;
          }
        }
      }
    }
  }
}

CDBOutput CDBArbiter::build(const ALU &ALUModule, const LQ &LQModule,
                            const SquashInfo &squash) {
  CDBCandidate aluCand{};
  if (!ALUModule.isEmpty()) {
    aluCand.valid = true;
    aluCand.result.value = ALUModule.headValue();
    aluCand.result.robTag = ALUModule.headRobTag();
    aluCand.result.isControl = ALUModule.headIsControl();
  }
  CDBCandidate lsqCand{};
  auto lsqCDBDetect = LQModule.CDBDetect();
  if (lsqCDBDetect != -1) {
    lsqCand.valid = true;
    lsqCand.memIndex = static_cast<uint8_t>(lsqCDBDetect);
    lsqCand.result.robTag = LQModule.getRobTag(lsqCDBDetect);
    lsqCand.result.value = LQModule.getValue(lsqCDBDetect);
  }
  return arbitrate(aluCand, lsqCand, squash);
}

CDBBus CDBBus::build(const CDBOutput &cdbOut, const SquashInfo &squashDetect) {
  CDBBus cdbBus{};
  if (cdbOut.valid) {
    auto &r = cdbOut.result;
    bool guard = !squashDetect.needSquash ||
                 ROB::isOlder(r.robTag, squashDetect.SquashTag);
    cdbBus.lsqSetCDB = guard && cdbOut.memGranted;
    cdbBus.memIndex = cdbOut.memIndex;
  }
  return cdbBus;
}

CDBOutput CDBArbiter::arbitrate(const CDBCandidate &aluCandidate,
                                const CDBCandidate &lsqCandidate,
                                const SquashInfo &squash) {
  bool needSquash = squash.needSquash;
  uint8_t squashTag = squash.SquashTag;
  bool aluValid = aluCandidate.valid;
  ArithmeticCalculateResult aluResult = aluCandidate.result;
  if (aluValid && needSquash && !ROB::isOlder(aluResult.robTag, squashTag))
    aluValid = false;

  bool lsqValid = lsqCandidate.valid;
  ArithmeticCalculateResult lsqResult = lsqCandidate.result;
  if (lsqValid && needSquash && !ROB::isOlder(lsqResult.robTag, squashTag))
    lsqValid = false;
  CDBOutput out = {};

  if (!aluValid && !lsqValid)
    return out;

  if (aluValid && !lsqValid) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
    return out;
  }

  if (!aluValid && lsqValid) {
    out.result = lsqResult;
    out.valid = true;
    out.memGranted = true;
    out.memIndex = lsqCandidate.memIndex;
    return out;
  }

  if (ROB::isOlder(aluResult.robTag, lsqResult.robTag) ||
      aluResult.robTag == lsqResult.robTag) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
  } else {
    out.result = lsqResult;
    out.valid = true;
    out.memGranted = true;
    out.memIndex = lsqCandidate.memIndex;
  }
  return out;
}

DispatchBus DispatchArbiter::arbitrate(const RSUnit &RS, const ALU &ALU,
                                       const AGU &AGU, const BRU &BRU,
                                       const ROB &ROB, const PRF &PRF,
                                       const SquashInfo &squash) {
  DispatchBus dispatch;
  auto opReady = [&PRF](const Operand &op) {
    return PRF.isOperandReady(op);
  };
  if (!ALU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = RS.integerRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
        }
      }
    }
    if (foundAny) {
      dispatch.alu.rsIndex = best;
      dispatch.alu.robTag = bestTag;
      dispatch.alu.rsType = RSType::Integer;
      dispatch.alu.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.alu.valid = false;
    }
  }
  if (!AGU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    RSType bestType = RSType::Load;
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = RS.loadRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
          bestType = RSType::Load;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = RS.storeAddressRS[i];
      if (!rs.free && opReady(rs.src1)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
          bestType = RSType::StoreAddr;
        }
      }
    }
    if (foundAny) {
      dispatch.agu.rsIndex = best;
      dispatch.agu.robTag = bestTag;
      dispatch.agu.rsType = bestType;
      dispatch.agu.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.agu.valid = false;
    }
  }
  if (!BRU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = RS.branchRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
        }
      }
    }
    if (foundAny) {
      dispatch.bru.rsIndex = best;
      dispatch.bru.robTag = bestTag;
      dispatch.bru.rsType = RSType::Branch;
      dispatch.bru.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.bru.valid = false;
    }
  }
  return dispatch;
}

MemDispatchDecision MemRequestArbiter::arbitrate(const LQ &LQ, const SQ &SQ,
                                                 const ROB &rob,
                                                 const DMEM &dmem,
                                                 const SquashInfo &squash) {
  MemDispatchDecision memDecision{};
  if (!dmem.isBusy() && !SQ.isEmpty()) {
    auto storeTag = SQ.headRobTag();
    bool committed = rob.isEmpty() || ROB::isOlder(storeTag, rob.getHead());
    bool atHeadReady = !committed && SQ.headRobTag() == rob.getHead() &&
                       rob.isCommitReadyAt(((SQ.headRobTag() & 0x3F)));
    if (committed || atHeadReady) {
      MemRequest newRequest{};
      newRequest.address = SQ.getAddress(SQ.getHead());
      newRequest.value = SQ.getValue(SQ.getHead());
      newRequest.n_bytes = SQ.getNBytes(SQ.getHead());
      newRequest.op = Operation::Store;
      newRequest.robTag = storeTag;
      newRequest.memIndex = static_cast<uint8_t>(SQ.getHead() | MEM_STORE_BIT);
      memDecision.valid = true;
      memDecision.request = newRequest;
    }
  }
  if (memDecision.valid)
    return memDecision;
  auto loadIndex = LQ.LoadDetect();
  if (loadIndex != 0xFFFFFFFF && !dmem.isBusy()) {
    auto loadAddr = LQ.getAddress(loadIndex);
    auto loadTag = LQ.getRobTag(loadIndex);
    if (SQ.canDispatchLoad(loadAddr, loadTag)) {
      MemRequest newRequest{};
      newRequest.address = LQ.getAddress(loadIndex);
      newRequest.isSigned = !LQ.getIsUnsigned(loadIndex);
      newRequest.n_bytes = LQ.getNBytes(loadIndex);
      newRequest.op = Operation::Load;
      newRequest.robTag = LQ.getRobTag(loadIndex);
      newRequest.memIndex = static_cast<uint8_t>(loadIndex);
      if (!squash.needSquash ||
          (squash.needSquash &&
           ROB::isOlder(newRequest.robTag, squash.SquashTag))) {
        memDecision.valid = true;
        memDecision.request = newRequest;
      }
    }
  }
  return memDecision;
}