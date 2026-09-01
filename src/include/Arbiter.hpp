#pragma once
#include "AGU.hpp"
#include "ALU.hpp"
#include "BRU.hpp"
#include "DCache.hpp"
#include "Decoder.hpp"
#include "LQ.hpp"
#include "PRF.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "SQ.hpp"
#include "common.hpp"
#include <cstdint>
struct FlushRequest {
  SquashInfo requestArgs;
  bool valid;
};

struct CDBCandidate {
  bool valid = false;
  ArithmeticCalculateResult result{};
  uint8_t memIndex = 0;
};

struct systemState;
class ROB;

// FlushArbiter owns its queue and the whole squash flow: stage 1 consumes
// the accepted squash (clear), stage 2 detects BRU branch mispredicts,
// stage 3 detects CDB JALR mispredicts, stage 4 detects MDP load violations
// (store address resolves against younger executed loads) -- all reads from
// the snapshots (BRUModule head, cdbOut, ROBModule, AGUModule head store,
// LQModule/SQModule), all writes to its own queue (receive).
struct FlushArbiterInput {
  const BRU &BRUModule;
  const ROB &ROBModule;
  const AGU &AGUModule;
  const LQ &LQModule;
  const SQ &SQModule;
  CDBOutput cdbOut;
  SquashInfo squashDetect;
  FlushArbiterInput(const BRU &bru, const ROB &rob, const AGU &agu,
                    const LQ &lq, const SQ &sq)
      : BRUModule(bru), ROBModule(rob), AGUModule(agu), LQModule(lq),
        SQModule(sq) {}
};

class FlushArbiter {
private:
  FlushRequest requests[FLUSHARBITER_CAP];
  void receive(SquashInfo request);
  void clear(uint8_t tag);

public:
  FlushArbiter();
  SquashInfo arbitResult() const;
  FlushRequest getRequest(int i) const;
  void tick(const FlushArbiterInput &, systemState &);
};

class CDBArbiter {
public:
  // build: collect the two CDB candidates from the module snapshots
  // (ALU head / LSQ CDBDetect) then arbitrate -- the whole read()-side
  // construction, producers stay unknown to the arbiter.
  static CDBOutput build(const ALU &, const LQ &, const SquashInfo &);
  // arbitrate between the two CDB candidates (ALU result / LSQ load value).
  // Candidates are plain data collected from the module snapshots in read();
  // the arbiter itself does not know the producers.
  static CDBOutput arbitrate(const CDBCandidate &aluCandidate,
                             const CDBCandidate &lsqCandidate,
                             const SquashInfo &squash);
};

class DispatchArbiter {
public:
  static DispatchBus arbitrate(const RSUnit &RS, const ALU &ALU, const AGU &AGU,
                               const BRU &BRU, const ROB &ROB, const PRF &PRF,
                               const SquashInfo &squash);
};

class MemRequestArbiter {
public:
  static MemDispatchDecision arbitrate(const LQ &LQ, const SQ &SQ,
                                       const ROB &rob, const DCache &dcache,
                                       const SquashInfo &squash);
};