#pragma once
#include <cstring>
#ifndef ARBITER_HPP
#define ARBITER_HPP
#include "ALU.hpp"
#include "LSQ.hpp"
#include "common.hpp"
#include <stdexcept>
class RATSEL {
public:
  struct PortPair {
    RATWritePort first;
    RATWritePort second;
  };

  static PortPair RATWrite(RATWritePort issuePort, RATWritePort commitPort) {
    if (issuePort.valid && commitPort.valid &&
        issuePort.reg == commitPort.reg) {
      return {issuePort, {}};
    }
    return {commitPort, issuePort};
  }
};

struct FlushRequest {
  SquashInfo requestArgs;
  bool valid;
};

class FlushArbiter {
private:
  FlushRequest requests[FLUSHARBITER_CAP];

public:
  FlushArbiter() { std::memset(this, 0, sizeof(*this)); }
  void receive(SquashInfo request) {
    for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
      if (!requests[i].valid) {
        requests[i].valid = true;
        requests[i].requestArgs = request;
        return;
      }
    }
    throw std::runtime_error("flush arbiter overload!");
  }
  SquashInfo arbitResult() const {
    SquashInfo result{};
    int minTag = ~0u >> 1;
    for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
      if (requests[i].valid) {
        if (requests[i].requestArgs.SquashTag < minTag) {
          result = requests[i].requestArgs;
          minTag = requests[i].requestArgs.SquashTag;
        }
      }
    }
    return result;
  }
  void clear(int tag) {
    for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
      if (requests[i].valid) {
        if (requests[i].requestArgs.SquashTag >= tag) {
          requests[i].valid = false;
        }
      }
    }
  }
  FlushRequest getRequest(int i) const { return requests[i]; }
};

class CDBArbiter {
public:
  static CDBOutput arbitrate(const ALU &ALUModule, const LSQ &LSQModule,
                             const SquashInfo &squash) {
    bool needSquash = squash.needSquash;
    int squashTag = squash.SquashTag;
    bool aluValid = !ALUModule.isEmpty();
    ExecuteResult aluResult = aluValid ? ALUModule.peek() : ExecuteResult{};
    if (aluValid && needSquash && aluResult.robTag > squashTag)
      aluValid = false;

    auto lsqCDBDetect = LSQModule.CDBDetect();
    bool lsqValid = lsqCDBDetect != -1;
    ExecuteResult lsqResult{};
    if (lsqValid) {
      auto lsqEntry = LSQModule.getEntry(lsqCDBDetect);
      lsqResult.isAddress = false;
      lsqResult.robTag = lsqEntry.ROBTag;
      lsqResult.value = lsqEntry.value;
      if (needSquash && lsqEntry.ROBTag > squashTag)
        lsqValid = false;
    }
    CDBOutput out = {{0, 0, false}, false, false, false};

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
      return out;
    }

    if (aluResult.robTag <= lsqResult.robTag) {
      out.result = aluResult;
      out.valid = true;
      out.aluGranted = true;
    } else {
      out.result = lsqResult;
      out.valid = true;
      out.lsqGranted = true;
    }
    return out;
  }
};
#endif // ARBITER_HPP
