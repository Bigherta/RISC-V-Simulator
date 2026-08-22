#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "AGU.hpp"
#include "ALU.hpp"
#include "IssueArbiter.hpp"
#include "Arbiter.hpp"
#include "BRU.hpp"
#include "BPU.hpp"
#include "DMEM.hpp"
#include "Decoder.hpp"
#include "FetchQueue.hpp"
#include "FetchUnit.hpp"
#include "ICache.hpp"
#include "IMEM.hpp"
#include "LQ.hpp"
#include "SQ.hpp"
#include "Memory.hpp"
#include "PRF.hpp"
#include "RAT.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "common.hpp"
#include <cstring>

struct systemState {
  RSUnit RSModule;
  RAT RATModule;
  ROB ROBModule;
  ALU ALUModule;
  AGU AGUModule;
  BRU BRUModule;
  LQ LQModule;
  SQ SQModule;
  FetchQueue FQModule;
  ICache ICacheModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  DMEM DMEMModule;
  IMEM IMEMModule;
  BPU BPUModule;
  FlushArbiter flushArbiter;
  FetchUnit FetchUnitModule;

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
  LQ LQModule;
  SQ SQModule;
  FetchQueue FQModule;
  ICache ICacheModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  DMEM DMEMModule;
  IMEM IMEMModule;
  BPU BPUModule;
  FlushArbiter flushArbiter;
  FetchUnit FetchUnitModule;
  CDBOutput cdbOut;
  IssuePacket issuePacket;
  AGUInput aguInput{RSModule, PRFModule};
  ALUInput aluInput{RSModule, PRFModule};
  BRUInput bruInput{ROBModule, RSModule, PRFModule};
  BPUpdateInput bpInput{BRUModule, ROBModule};
  DMEMInput dmemInput{};
  DecodeInput decodeInput{FQModule, issuePacket};
  LQInput lqInput{AGUModule, RSModule, ROBModule, DMEMModule, SQModule,
                   issuePacket};
  SQInput sqInput{AGUModule, RSModule, PRFModule, ROBModule, DMEMModule,
                   LQModule, issuePacket};
  RSInput rsInput{issuePacket, PRFModule};
  ROBInput robInput{BRUModule, LQModule, SQModule, issuePacket};
  PRFInput prfInput{LQModule, SQModule, ROBModule, issuePacket};
  RATInput ratInput{ROBModule, issuePacket};
  FlushArbiterInput flarbInput{BRUModule, ROBModule, AGUModule, LQModule,
                                SQModule};
  IssueArbiterInput isarbInput{DecodeUnitModule, ROBModule, RSModule,
                                RATModule,        PRFModule, LQModule,
                                SQModule};
  SquashInfo squashDetect;
  FetchDecision fetchDecision;
  FetchUnitInput fetchUnitInput;
  IMEMInput imemInput;
  ICacheInput icacheInput{};
  FQInput fqInput{ICacheModule, DecodeUnitModule};
public:
  CPU(Memory mem);
  void comb();
  void run();
};

#endif // CPU_HPP