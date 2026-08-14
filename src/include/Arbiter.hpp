#pragma once
#include <cstdint>
#ifndef ARBITER_HPP
#define ARBITER_HPP
#include "common.hpp"
struct ALU;
struct LSQ;
struct FlushRequest {
  SquashInfo requestArgs;
  bool valid;
};

class FlushArbiter {
private:
  FlushRequest requests[FLUSHARBITER_CAP];

public:
  FlushArbiter();
  void receive(SquashInfo request);
  SquashInfo arbitResult() const;
  void clear(uint64_t seq);
  FlushRequest getRequest(int i) const;
};

class CDBArbiter {
public:
  static CDBOutput arbitrate(const ALU &ALUModule, const LSQ &LSQModule,
                             const SquashInfo &squash);
};
#endif // ARBITER_HPP
