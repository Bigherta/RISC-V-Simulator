#pragma once
#include "AGU.hpp"
#include "ALU.hpp"
#include "BRU.hpp"
#include "Decoder.hpp"
#include "PRF.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "common.hpp"
#include <cstdint>
struct FlushRequest {
  SquashInfo requestArgs;
  bool valid;
};

struct CDBCandidate {
  bool valid = false;
  ArithmeticCalculateResult result{};
};

struct systemState;
class ROB;

// FlushArbiter owns its queue and the whole squash flow: 段1 consumes the
// accepted squash (clear), 段2 detects BRU branch mispredicts, 段3 detects
// CDB JALR mispredicts -- all reads from the snapshots (BRUModule head,
// cdbArbiter, ROBModule), all writes to its own queue (receive). The BRU/CDB
// producers' writeBack parts (remove / setROBCommitReady) stay in their own
// ticks.
struct FlushArbiterInput {
  const BRU &BRUModule;
  const ROB &ROBModule;
  CDBOutput cdbArbiter;
  SquashInfo squashDetect;
  FlushArbiterInput(const BRU &bru, const ROB &rob)
      : BRUModule(bru), ROBModule(rob) {}
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
  void tick(const FlushArbiterInput &, systemState &);
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

class DispatchArbiter {
public:
  static DispatchBus arbitrate(const RSUnit &RS, const ALU &ALU, const AGU &AGU,
                               const BRU &BRU, const ROB &ROB,
                               const SquashInfo &squash);
};