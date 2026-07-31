#pragma once
#include "CDB.hpp"
#ifndef CPU_HPP
#define CPU_HPP
#include "ALU.hpp"
#include "LSQ.hpp"
#include "Memory.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "Register.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState {
  ReservationStation IntegerRS[8];
  ReservationStation StoreRS[4];
  StoreMicroReservationStation MicroStoreRS[4];
  ReservationStation LoadRS[4];
  RegCluster REGModule;
  ROB ROBModule;
  ALU ALUModule;
  LSQ LSQModule;
  CDB CDBModule;
  Memory InstructMem, DataMem;
  uint32_t programCounter;
  bool PCWriteEnable;

  systemState() : programCounter(0), PCWriteEnable(false) {}
  systemState(Memory mem)
      : InstructMem(mem), DataMem(mem), programCounter(0), PCWriteEnable(false) {}
  systemState(const systemState &other) {
    std::memcpy(IntegerRS, other.IntegerRS, sizeof(IntegerRS));
    std::memcpy(StoreRS, other.StoreRS, sizeof(StoreRS));
    std::memcpy(MicroStoreRS, other.MicroStoreRS, sizeof(MicroStoreRS));
    std::memcpy(LoadRS, other.LoadRS, sizeof(LoadRS));
    ROBModule = other.ROBModule;
    ALUModule = other.ALUModule;
    LSQModule = other.LSQModule;
    CDBModule = other.CDBModule;
    REGModule = other.REGModule;
    InstructMem = other.InstructMem;
    DataMem = other.DataMem;
    programCounter = other.programCounter;
    PCWriteEnable = other.PCWriteEnable;
  }
  systemState &operator=(const systemState &other) {
    if (this == &other)
      return *this;
    std::memcpy(IntegerRS, other.IntegerRS, sizeof(IntegerRS));
    std::memcpy(StoreRS, other.StoreRS, sizeof(StoreRS));
    std::memcpy(MicroStoreRS, other.MicroStoreRS, sizeof(MicroStoreRS));
    std::memcpy(LoadRS, other.LoadRS, sizeof(LoadRS));
    ROBModule = other.ROBModule;
    ALUModule = other.ALUModule;
    LSQModule = other.LSQModule;
    CDBModule = other.CDBModule;
    REGModule = other.REGModule;
    InstructMem = other.InstructMem;
    DataMem = other.DataMem;
    programCounter = other.programCounter;
    PCWriteEnable = other.PCWriteEnable;
    return *this;
  }
};

class CPU {
private:
  systemState curCPUstate;
  systemState nextCPUstate;
  friend struct CPU_Tester;
  friend struct ReorderTester;

public:
  CPU(Memory mem);
  int issue();
  bool issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk);
  bool issue_IntegerU(Instruct inst, bool has_PC);
  bool issue_Load(Instruct inst, int n_bytes, bool isUnsigned);
  bool issue_Store(Instruct inst, int n_bytes);
  static Operation decodeOp(Instruct inst);
  void execute();
  void apply_B_operation(Instruct inst);
  void apply_J_operation(Instruct inst);
  void apply_U_operation(Instruct inst);
  void writeBack();
  void commit();
  void run();
};

#endif // CPU_HPP