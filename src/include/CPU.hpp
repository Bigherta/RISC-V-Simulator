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
#include "LQ.hpp"
#include "SQ.hpp"
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
  LQ LQModule;
  SQ SQModule;
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
  LQ LQModule;
  SQ SQModule;
  FetchQueue FQModule;
  DecodeUnit DecodeUnitModule;
  PRF PRFModule;
  DMEM DMEMModule;
  IMEM IMEMModule;
  BranchPredictor BPModule;
  FlushArbiter flushArbiter;
  CDBOutput cdbOut;
  IssuePacket issuePacket;
  AGUInput aguInput{RSModule};
  ALUInput aluInput{RSModule};
  BRUInput bruInput{ROBModule, RSModule};
  BPUpdateInput bpInput{BRUModule, ROBModule};
  DMEMInput dmemInput{};
  DecodeInput decodeInput{FQModule, issuePacket};
  LQInput lqInput{AGUModule, RSModule, ROBModule, DMEMModule, SQModule,
                  issuePacket};
  SQInput sqInput{AGUModule, RSModule, ROBModule, DMEMModule, LQModule,
                  issuePacket};
  RSInput rsInput{ROBModule, issuePacket};
  ROBInput robInput{BRUModule, LQModule, SQModule, issuePacket};
  PRFInput prfInput{LQModule, ROBModule, issuePacket};
  RATInput ratInput{ROBModule, issuePacket};
  FlushArbiterInput flarbInput{BRUModule, ROBModule};
  IssueArbiterInput isarbInput{DecodeUnitModule, ROBModule, RSModule,
                               RATModule,        PRFModule, LQModule,
                               SQModule};
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