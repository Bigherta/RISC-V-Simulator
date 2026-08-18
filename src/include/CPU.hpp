#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "AGU.hpp"
#include "ALU.hpp"
#include "IssueArbiter.hpp"
#include "Arbiter.hpp"
#include "BRU.hpp"
#include "BranchPredictor.hpp"
#include "DMEM.hpp"
#include "Decoder.hpp"
#include "FetchQueue.hpp"
#include "IMEM.hpp"
#include "LSQ.hpp"
#include "Memory.hpp"
#include "PRF.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState {
  RSUnit RSModule;
  RAT RATModule;
  ROB ROBModule;
  ALU ALUModule;
  AGU AGUModule;
  BRU BRUModule;
  LSQ LSQModule;
  FetchQueue FQModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  DMEM DMEMModule;
  IMEM IMEMModule;
  BranchPredictor BPModule;
  FlushArbiter flushArbiter;

  systemState() = default;
  systemState(Memory mem)
      : DMEMModule(mem), IMEMModule(mem) {}
};

class CPU {
private:
  systemState CPUstate;
  IMEM InstructMem;
  friend struct ReorderTester;
  RSUnit RSModule;
  RAT RATModule;
  ROB ROBModule;
  ALU ALUModule;
  AGU AGUModule;
  BRU BRUModule;
  LSQ LSQModule;
  FetchQueue FQModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  DMEM DMEMModule;
  IMEM IMEMModule;
  BranchPredictor BPModule;
  FlushArbiter flushArbiter;
  CDBOutput cdbOut;
  IssuePacket issuePacket;
  AGUInput aguInput{LSQModule, RSModule};
  ALUInput aluInput{RSModule};
  BRUInput bruInput{ROBModule, RSModule};
  BPUpdateInput bpInput{BRUModule, ROBModule};
  DMEMInput dmemInput{LSQModule};
  DecodeInput decodeInput{FQModule, issuePacket};
  LSQInput lsqInput{AGUModule, RSModule, ROBModule, DMEMModule, issuePacket};
  RSInput rsInput{ROBModule, issuePacket};
  ROBInput robInput{BRUModule, LSQModule, issuePacket};
  PRFInput prfInput{LSQModule, ROBModule, issuePacket};
  RATInput ratInput{ROBModule, issuePacket};
  FlushArbiterInput flarbInput{BRUModule, ROBModule};
  IssueArbiterInput isarbInput{DecodeUnitModule, ROBModule, RSModule,
                               RATModule,        PRFModule, LSQModule};
  SquashInfo squashDetect;
  FetchDecision fetchDecision;
  IMEMInput imemInput{FQModule};
  FQInput fqInput{IMEMModule, DecodeUnitModule};
public:
  CPU(Memory mem);
  void comb();
  void run();
};

#endif // CPU_HPP