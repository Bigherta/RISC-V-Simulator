#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "ALU.hpp"
#include "Arbiter.hpp"
#include "BRU.hpp"
#include "INQ.hpp"
#include "LSQ.hpp"
#include "Memory.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "Register.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState {
  RSCluster RSModule;
  RegCluster REGModule;
  ROB ROBModule;
  ALU ALUModule;
  BRU BRUModule;
  LSQ LSQModule;
  INQ INQModule;
  Memory DataMem;
  FlushArbiter flushArbiter;
  uint32_t programCounter;
  SquashInfo squashDetect;
  bool haltFetched = false;
  bool haltCommitted = false;
  int haltRd = -1;

  systemState() : programCounter(0) {}
  systemState(Memory mem) : DataMem(mem), programCounter(0) {}
};

class CPU {
private:
  systemState CPUstate;
  Memory InstructMem;
  friend struct CPU_Tester;
  friend struct ReorderTester;
  friend struct Reorder720Tester;
  friend int debug_trace_main();
  friend void issue_from_inq(CPU &cpu, uint32_t raw, int pc);
  friend int debug_trace_main();
  RSCluster RSModule;
  RegCluster REGModule;
  ROB ROBModule;
  ALU ALUModule;
  BRU BRUModule;
  LSQ LSQModule;
  INQ INQModule;
  Memory DataMem;
  FlushArbiter flushArbiter;
  CDBOutput cdbArbiter;
  uint32_t programCounter;
  SquashInfo squashDetect;
  bool haltFetched = false;
  bool haltCommitted = false;
  int haltRd = -1;
  uint64_t branchTotal = 0;
  uint64_t branchCorrect = 0;

public:
  CPU(Memory mem);
  void read();
  void fetch();
  void decode();
  void issue();
  IssueResult issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk,
                              bool isControl);
  IssueResult issue_UandJ(Instruct inst, bool has_PC, bool isControl = false);
  IssueResult issue_Load(Instruct inst, int n_bytes, bool isUnsigned);
  IssueResult issue_Store(Instruct inst, int n_bytes);
  IssueResult issue_B(Instruct inst);
  static Operation decodeOp(Instruct inst);
  void execute();
  void writeBack();
  CDBBypassResult CDBBypass(int robTag) const;
  void CDBBroadcast(int tag, int value);
  void commit();
  void flush();
  void run();
};

#endif // CPU_HPP