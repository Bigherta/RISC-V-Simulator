#pragma once
#include "Decoder.hpp"
#include "RS.hpp"
#include "ROB.hpp"
#include "LQ.hpp"
#include "SQ.hpp"
struct IssuePacket {
  bool valid = false;
  bool allocDest = false;
  int phy = -1;
  int robIndex = -1;
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
  CDBOutput cdbOut;
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
  static CDBBypassResult CDBBypass(const IssueArbiterInput &, int phy);

public:
  static IssuePacket build(const IssueArbiterInput &);
};