#pragma once
#include <cstdint>
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
    while (pos < w && requests[pos].requestArgs.SquashSeq <
                         request.SquashSeq)
      ++pos;
    for (int i = FLUSHARBITER_CAP - 1; i > pos; --i)
      requests[i] = requests[i - 1];
    requests[pos].valid = true;
    requests[pos].requestArgs = request;
  }
  SquashInfo arbitResult() const {
    SquashInfo result{};
    uint64_t minSeq = ~0ull >> 1;
    for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
      if (requests[i].valid) {
        if (requests[i].requestArgs.SquashSeq < minSeq) {
          result = requests[i].requestArgs;
          minSeq = requests[i].requestArgs.SquashSeq;
        }
      }
    }
    return result;
  }
  void clear(uint64_t seq) {
    for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
      if (requests[i].valid) {
        if (requests[i].requestArgs.SquashSeq >= seq) {
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
    uint64_t squashSeq = squash.SquashSeq;
    bool aluValid = !ALUModule.isEmpty();
    ExecuteResult aluResult = aluValid ? ALUModule.peek() : ExecuteResult{};
    if (aluValid && needSquash && aluResult.robSeq > squashSeq)
      aluValid = false;

    auto lsqCDBDetect = LSQModule.CDBDetect();
    bool lsqValid = lsqCDBDetect != -1;
    ExecuteResult lsqResult{};
    if (lsqValid) {
      lsqResult.isAddress = false;
      lsqResult.robIndex = LSQModule.getRobIndex(lsqCDBDetect);
      lsqResult.robSeq = LSQModule.getRobSeq(lsqCDBDetect);
      lsqResult.value = LSQModule.getValue(lsqCDBDetect);
      if (needSquash && lsqResult.robSeq > squashSeq)
        lsqValid = false;
    }
    CDBOutput out = {{0, 0, 0, false, false}, false, false, false};

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

    if (aluResult.robSeq <= lsqResult.robSeq) {
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
