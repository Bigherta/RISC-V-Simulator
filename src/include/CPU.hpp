#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "ALU.hpp"
#include "AGU.hpp"
#include "Arbiter.hpp"
#include "BRU.hpp"
#include "Decoder.hpp"
#include "FetchQueue.hpp"
#include "LSQ.hpp"
#include "PRF.hpp"
#include "Memory.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "Register.hpp"
#include "BranchPredictor.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState {
  IntegerRS IntegerRSModule;
  StoreAddressRS StoreAddressRSModule;
  StoreValueRS StoreValueRSModule;
  LoadRS LoadRSModule;
  BranchRS BranchRSModule;
  RegCluster REGModule;
  ROB ROBModule;
  ALU ALUModule;
  AGU AGUModule;
  BRU BRUModule;
  LSQ LSQModule;
  FetchQueue FQModule;
  InstructQueue IQModule;
  PRF PRFModule;
  Memory DataMem;
  BranchPredictor BPModule;
  FlushArbiter flushArbiter;
  uint32_t programCounter;
  bool haltFetched = false;
  bool haltCommitted = false;
  int haltRd = -1;
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;

  systemState() : programCounter(0) {}
  systemState(Memory mem) : DataMem(mem), programCounter(0) {}
};

class CPU {
private:
  systemState CPUstate;
  Memory InstructMem;
  friend struct ReorderTester;
  IntegerRS IntegerRSModule;
  StoreAddressRS StoreAddressRSModule;
  StoreValueRS StoreValueRSModule;
  LoadRS LoadRSModule;
  BranchRS BranchRSModule;
  RegCluster REGModule;
  ROB ROBModule;
  ALU ALUModule;
  AGU AGUModule;
  BRU BRUModule;
  LSQ LSQModule;
  FetchQueue FQModule;
  InstructQueue IQModule;
  PRF PRFModule;
  Memory DataMem;
  BranchPredictor BPModule;
  FlushArbiter flushArbiter;
  CDBOutput cdbArbiter;
  AGUInput aguInput{ROBModule, LSQModule};
  BRUInput bruInput{ROBModule};
  BPUpdateInput bpInput{BRUModule, ROBModule};
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
  void decode();
  void issue();
  int issue_IntegerRS(const Uop &inst, bool has_rs2, bool imm_as_vk,
                      bool isControl);
  int issue_UandJ(const Uop &inst, bool has_PC, bool isControl = false);
  int issue_Load(const Uop &inst, int n_bytes, bool isUnsigned);
  int issue_Store(const Uop &inst, int n_bytes);
  int issue_B(const Uop &inst);
  static Operation decodeOp(const Uop &inst);
  void execute();
  void writeBack();
  void CDBBroadcast(int robIndex, int value);
  CDBBypassResult CDBBypass(int robIndex) const;
  void commit();
  void flush();
  void run();
};

#endif // CPU_HPP