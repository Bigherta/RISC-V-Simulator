#include "../include/Arbiter.hpp"
#include "../include/CPU.hpp"
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

DispatchBus DispatchArbiter::arbitrate(const RSUnit &RS, const ALU &ALU,
                                       const AGU &AGU, const BRU &BRU,
                                       const ROB &ROB,
                                       const SquashInfo &squash) {
  DispatchBus dispatch;
  // 与原 ALU/AGU/BRU 段1 的选择逻辑逐字等价（同一快照求值一次）：
  // gate（FU isFull）→ 最老就绪（ROB seq 比较）→ squash 过滤。
  if (!ALU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint64_t bestSeq = 0;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = RS.integerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        auto seq = ROB.getSeq(rs.robIndex);
        if (!foundAny || seq < bestSeq) {
          foundAny = true;
          best = i;
          bestSeq = seq;
        }
      }
    }
    if (foundAny) {
      dispatch.alu.rsIndex = best;
      dispatch.alu.robIndex = RS.integerRS[best].robIndex;
      dispatch.alu.robSeq = bestSeq;
      dispatch.alu.rsType = RSType::Integer;
      dispatch.alu.valid = true;
      if (squash.needSquash && bestSeq >= squash.SquashSeq)
        dispatch.alu.valid = false;
    }
  }
  // AGU：跨 loadRS 与 storeAddressRS 统一选最老（单一 foundAny，原段1 语义；
  // storeAddress 只查 qj==-1——qk 恒为 -1，与只查 qj 等价）。
  if (!AGU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint64_t bestSeq = 0;
    int bestRobIndex = -1;
    RSType bestType = RSType::Load;
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = RS.loadRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        auto seq = ROB.getSeq(rs.robIndex);
        if (!foundAny || seq < bestSeq) {
          foundAny = true;
          best = i;
          bestSeq = seq;
          bestRobIndex = rs.robIndex;
          bestType = RSType::Load;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = RS.storeAddressRS[i];
      if (!rs.free && rs.qj == -1) {
        auto seq = ROB.getSeq(rs.robIndex);
        if (!foundAny || seq < bestSeq) {
          foundAny = true;
          best = i;
          bestSeq = seq;
          bestRobIndex = rs.robIndex;
          bestType = RSType::StoreAddr;
        }
      }
    }
    if (foundAny) {
      dispatch.agu.rsIndex = best;
      dispatch.agu.robIndex = bestRobIndex;
      dispatch.agu.robSeq = bestSeq;
      dispatch.agu.rsType = bestType;
      dispatch.agu.valid = true;
      if (squash.needSquash && bestSeq >= squash.SquashSeq)
        dispatch.agu.valid = false;
    }
  }
  if (!BRU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint64_t bestSeq = 0;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = RS.branchRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        auto seq = ROB.getSeq(rs.robIndex);
        if (!foundAny || seq < bestSeq) {
          foundAny = true;
          best = i;
          bestSeq = seq;
        }
      }
    }
    if (foundAny) {
      dispatch.bru.rsIndex = best;
      dispatch.bru.robIndex = RS.branchRS[best].robIndex;
      dispatch.bru.robSeq = bestSeq;
      dispatch.bru.rsType = RSType::Branch;
      dispatch.bru.valid = true;
      if (squash.needSquash && bestSeq >= squash.SquashSeq)
        dispatch.bru.valid = false;
    }
  }
  return dispatch;
}