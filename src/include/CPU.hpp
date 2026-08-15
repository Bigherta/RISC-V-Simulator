#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "AGU.hpp"
#include "ALU.hpp"
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
#include "ROB.hpp"
#include "RS.hpp"
#include "RAT.hpp"
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
  uint32_t programCounter;
  bool haltFetched = false;
  bool haltCommitted = false;
  int haltRd = -1;
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;

  systemState() : programCounter(0) {}
  systemState(Memory mem) : DMEMModule(mem), programCounter(0) {}
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
  CDBOutput cdbArbiter;
  AGUInput aguInput{LSQModule, RSModule};
  ALUInput aluInput{RSModule};
  BRUInput bruInput{ROBModule, RSModule};
  BPUpdateInput bpInput{BRUModule, ROBModule};
  DMEMInput dmemInput{LSQModule};
  DecodeInput decodeInput{FQModule};
  LSQInput lsqInput{AGUModule, RSModule, ROBModule, DMEMModule};
  RSInput rsInput{ROBModule};
  ROBInput robInput{BRUModule, LSQModule};
  PRFInput prfInput{LSQModule, ROBModule};
  uint32_t programCounter;
  SquashInfo squashDetect;
  bool haltFetched = false;
  bool haltCommitted = false;
  int haltRd = -1;
  bool checkPRFInvariant() const;

public:
  CPU(Memory mem);
  void read();
  void fetch();
  void issue();
  int issue_IntegerRS(const Uop &inst, bool has_rs2, bool imm_as_vk,
                      bool isControl);
  int issue_UandJ(const Uop &inst, bool has_PC, bool isControl = false);
  int issue_Load(const Uop &inst, int n_bytes, bool isUnsigned);
  int issue_Store(const Uop &inst, int n_bytes);
  int issue_B(const Uop &inst);
  static Operation decodeOp(const Uop &inst);
  CDBBypassResult CDBBypass(int robIndex) const;
  void flush();
  void run();
};

#endif // CPU_HPP