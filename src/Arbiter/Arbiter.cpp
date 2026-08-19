#include "../include/Arbiter.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
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
      if (!found || ROB::isOlder(requests[i].requestArgs.SquashTag, result.SquashTag)) {
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
                          input.ROBModule.getIndexByTag(brRobTag))) {
        if (debug::enabled(debug::TOPIC_BRANCH))
          debug::print("squash tag=%u pc=%u (from %u)\n", brRobTag, actualPC,
                       pcFrom);
        BranchSquash.needSquash = true;
        BranchSquash.SquashPC = actualPC;
        BranchSquash.SquashIndex = input.ROBModule.getIndexByTag(brRobTag);
        BranchSquash.SquashTag = brRobTag;
        BranchSquash.CkptId =
            input.ROBModule.getCkptId(BranchSquash.SquashIndex);
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
        if (pc != input.ROBModule.getPredictedPC(input.ROBModule.getIndexByTag(
                     cdbOut.result.robTag))) {
          if (debug::enabled(debug::TOPIC_BRANCH))
            debug::print("squash tag=%u pc=%u (jalr)\n",
                         cdbOut.result.robTag, pc);
          JumpSquash.needSquash = true;
          JumpSquash.SquashPC = pc;
          JumpSquash.SquashIndex =
              input.ROBModule.getIndexByTag(cdbOut.result.robTag);
          JumpSquash.SquashTag = cdbOut.result.robTag;
          JumpSquash.CkptId =
              input.ROBModule.getCkptId(JumpSquash.SquashIndex);
        }
        if (JumpSquash.needSquash)
          CPUstate.flushArbiter.receive(JumpSquash);
      }
    }
  }
}

CDBOutput CDBArbiter::build(const ALU &ALUModule, const LSQ &LSQModule,
                            const SquashInfo &squash) {
  CDBCandidate aluCand{};
  if (!ALUModule.isEmpty()) {
    aluCand.valid = true;
    aluCand.result.value = ALUModule.headValue();
    aluCand.result.robTag = ALUModule.headRobTag();
    aluCand.result.isControl = ALUModule.headIsControl();
  }
  CDBCandidate lsqCand{};
  auto lsqCDBDetect = LSQModule.CDBDetect();
  if (lsqCDBDetect != -1) {
    lsqCand.valid = true;
    lsqCand.lsqIndex = static_cast<uint8_t>(lsqCDBDetect);
    lsqCand.result.robTag = LSQModule.getRobTag(lsqCDBDetect);
    lsqCand.result.value = LSQModule.getValue(lsqCDBDetect);
  }
  return arbitrate(aluCand, lsqCand, squash);
}

CDBBus CDBBus::build(const CDBOutput &cdbOut, const ROB &ROBModule,
                     const PRF &PRFModule, const SquashInfo &squashDetect) {
  CDBBus cdbBus{};
  if (cdbOut.valid) {
    auto &r = cdbOut.result;
    bool guard =
        !squashDetect.needSquash ||
        ROB::isOlder(r.robTag, squashDetect.SquashTag);
    bool robOk = !ROBModule.isEmpty() &&
                 !ROB::isOlder(r.robTag, ROBModule.getHead());
    cdbBus.broadcastValid = guard && (!r.isControl || robOk);
    int newPhy =
        robOk ? ROBModule.getNewPhy(ROBModule.getIndexByTag(r.robTag)) : -1;
    cdbBus.broadcastValue =
        r.isControl
            ? (newPhy >= 0 ? PRFModule.getValue(newPhy) : 0)
            : r.value;
    cdbBus.lsqSetCDB = guard && cdbOut.lsqGranted;
    cdbBus.robTag = r.robTag;
    cdbBus.lsqIndex = cdbOut.lsqIndex;
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
  if (aluValid && needSquash &&
      !ROB::isOlder(aluResult.robTag, squashTag))
    aluValid = false;

  bool lsqValid = lsqCandidate.valid;
  ArithmeticCalculateResult lsqResult = lsqCandidate.result;
  if (lsqValid && needSquash &&
      !ROB::isOlder(lsqResult.robTag, squashTag))
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
    out.lsqGranted = true;
    out.lsqIndex = lsqCandidate.lsqIndex;
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
    out.lsqGranted = true;
    out.lsqIndex = lsqCandidate.lsqIndex;
  }
  return out;
}

DispatchBus DispatchArbiter::arbitrate(const RSUnit &RS, const ALU &ALU,
                                       const AGU &AGU, const BRU &BRU,
                                       const ROB &ROB,
                                       const SquashInfo &squash) {
  DispatchBus dispatch;
  if (!ALU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = RS.integerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
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
      if (squash.needSquash &&
          !ROB::isOlder(bestTag, squash.SquashTag))
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
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
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
      if (!rs.free && rs.qj == -1) {
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
      if (squash.needSquash &&
          !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.agu.valid = false;
    }
  }
  if (!BRU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = RS.branchRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
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
      if (squash.needSquash &&
          !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.bru.valid = false;
    }
  }
  return dispatch;
}