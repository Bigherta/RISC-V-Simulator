#pragma once
#include <cstdint>
#ifndef ARBITER_HPP
#define ARBITER_HPP
#include "common.hpp"
struct FlushRequest {
  SquashInfo requestArgs;
  bool valid;
};

struct CDBCandidate {
  bool valid = false;
  ArithmeticCalculateResult result{};
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
  // arbitrate between the two CDB candidates (ALU result / LSQ load value).
  // Candidates are plain data collected from the module snapshots in read();
  // the arbiter itself does not know the producers.
  static CDBOutput arbitrate(const CDBCandidate &aluCandidate,
                             const CDBCandidate &lsqCandidate,
                             const SquashInfo &squash);
};
#endif // ARBITER_HPP
