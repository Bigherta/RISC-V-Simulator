#pragma once
// StaticArbiter: the stateless arbiter family (pure combinational, zero
// flip-flops in RTL). CDBArbiter / DispatchArbiter / MemArbiter / IssueArbiter
// share one form: no members at all, every entry point is a static function of
// its module snapshots. CDBBus::build rides along here (it is a bus, not an
// arbiter -- its type declaration stays in common.hpp because LQ holds one by
// value). The stateful FlushArbiter lives separately in DynamicArbiter.hpp
// (registered queue vs always_comb is the stateful/stateless hardware boundary).
// NOTE: this tree is still the comb()/tick()/memcpy reference implementation --
// only the file layout follows RISC-V-Simulator-Template/, the classes here are
// plain C++ (no Register/Wire/dark::Module).
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

class DispatchArbiter {
public:
  static DispatchBus arbitrate(const RSUnit &RS, const ALU &ALU, const AGU &AGU,
                               const BRU &BRU, const ROB &ROB, const PRF &PRF,
                               const SquashInfo &squash);
};

// Named MemArbiter (was MemRequestArbiter) to match the template tree file
// family; one memory request per cycle, store over load.
class MemArbiter {
public:
  static MemDispatchDecision arbitrate(const LQ &LQ, const SQ &SQ,
                                       const ROB &rob, const DCache &dcache,
                                       const SquashInfo &squash);
};

struct IssuePacket {
  bool valid = false;
  bool allocDest = false;
  int phy = InvalidPhy; // valid only when allocDest; P0-dead sentinel domain
  uint8_t robTag = 0;
  ROBEntry robEntry;
  bool hasInteger = false;
  int integerSlot = -1;
  ReservationStation integerRS;
  bool hasLoad = false;
  int loadSlot = -1;
  LoadAddressRS loadRS;
  bool hasStore = false;
  int storeAddrSlot = -1, storeValueSlot = -1;
  StoreAddressRS storeAddrRS;
  StoreValueReservationStation storeValueRS;
  bool hasBranch = false;
  int branchSlot = -1;
  BranchReservationStation branchRS;
  bool isLoad = false, isStore = false, isControl = false, isHalt = false;
  int nBytes = 0;
  bool isUnsigned = false;
  uint32_t pc = 0;
};
struct DecodeUnit;
struct RAT;
struct PRF;
struct IssueArbiterInput {
  const DecodeUnit &DecodeUnitModule;
  const ROB &ROBModule;
  const RSUnit &RSModule;
  const RAT &RATModule;
  const PRF &PRFModule;
  const LQ &LQModule;
  const SQ &SQModule;
  SquashInfo squashDetect;
  IssueArbiterInput(const DecodeUnit &DecodeUnitModule, const ROB &ROBModule,
                    const RSUnit &RSModule, const RAT &RATModule,
                    const PRF &PRFModule, const LQ &LQModule, const SQ &SQModule)
      : DecodeUnitModule(DecodeUnitModule), RATModule(RATModule),
        ROBModule(ROBModule), RSModule(RSModule), PRFModule(PRFModule),
        LQModule(LQModule), SQModule(SQModule) {}
};
struct IssueArbiter {
private:
  static IssuePacket issue_IntegerRS(const IssueArbiterInput &,
                                     const UopView &inst,
                                     bool has_rs2, bool imm_as_vk,
                                     bool isControl);
  static IssuePacket issue_UandJ(const IssueArbiterInput &,
                                 const UopView &inst,
                                 bool has_PC, bool isControl = false);
  static IssuePacket issue_Load(const IssueArbiterInput &, const UopView &inst,
                                int n_bytes, bool isUnsigned);
  static IssuePacket issue_Store(const IssueArbiterInput &, const UopView &inst,
                                 int n_bytes);
  static IssuePacket issue_B(const IssueArbiterInput &, const UopView &inst);
  static Operation decodeOp(const UopView &inst);

public:
  static IssuePacket build(const IssueArbiterInput &);
};
