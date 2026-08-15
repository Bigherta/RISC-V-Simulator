#include "../include/Arbiter.hpp"
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
  while (pos < w && requests[pos].requestArgs.SquashSeq < request.SquashSeq)
    ++pos;
  for (int i = FLUSHARBITER_CAP - 1; i > pos; --i)
    requests[i] = requests[i - 1];
  requests[pos].valid = true;
  requests[pos].requestArgs = request;
}

SquashInfo FlushArbiter::arbitResult() const {
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

void FlushArbiter::clear(uint64_t seq) {
  for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
    if (requests[i].valid) {
      if (requests[i].requestArgs.SquashSeq >= seq) {
        requests[i].valid = false;
      }
    }
  }
}

FlushRequest FlushArbiter::getRequest(int i) const { return requests[i]; }

CDBOutput CDBArbiter::arbitrate(const CDBCandidate &aluCandidate,
                                const CDBCandidate &lsqCandidate,
                                const SquashInfo &squash) {
  bool needSquash = squash.needSquash;
  uint64_t squashSeq = squash.SquashSeq;
  bool aluValid = aluCandidate.valid;
  ArithmeticCalculateResult aluResult = aluCandidate.result;
  if (aluValid && needSquash && aluResult.robSeq > squashSeq)
    aluValid = false;

  bool lsqValid = lsqCandidate.valid;
  ArithmeticCalculateResult lsqResult = lsqCandidate.result;
  if (lsqValid && needSquash && lsqResult.robSeq > squashSeq)
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