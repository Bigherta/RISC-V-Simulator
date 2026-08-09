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
    while (pos < w && requests[pos].requestArgs.SquashTag <
                         request.SquashTag)
      ++pos;
    for (int i = FLUSHARBITER_CAP - 1; i > pos; --i)
      requests[i] = requests[i - 1];
    requests[pos].valid = true;
    requests[pos].requestArgs = request;
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
      lsqResult.isAddress = false;
      lsqResult.robTag = LSQModule.getROBTag(lsqCDBDetect);
      lsqResult.value = LSQModule.getValue(lsqCDBDetect);
      if (needSquash && lsqResult.robTag > squashTag)
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
