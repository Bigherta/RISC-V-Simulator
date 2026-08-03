#pragma once
#ifndef ARBITER_HPP
#define ARBITER_HPP
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
};
#endif // ARBITER_HPP
