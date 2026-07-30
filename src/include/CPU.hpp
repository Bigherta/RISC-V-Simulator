#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "Memory.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "Register.hpp"
#include "ALU.hpp"
#include "common.hpp"
#include <cstdint>
#include <cstring>
constexpr int INTEGERRS_CAP = 8;
constexpr int STORERS_CAP = 4;
constexpr int LOADRS_CAP = 4;
struct systemState {
  Register reg[32];
  int RegisterTable[32];
  ReservationStation IntegerRS[8];
  ReservationStation StoreRS[4];
  ReservationStation LoadRS[4];
  ROB ROBModule;
  ALU ALUModule;
  Memory InstructMem, DataMem;
  systemState() {
    std::memset(RegisterTable, 0xFF, sizeof(RegisterTable));
  }
  systemState(Memory mem) : InstructMem(mem), DataMem(mem) {
    std::memset(RegisterTable, 0xFF, sizeof(RegisterTable));
  }
  systemState(const systemState &other) {
    std::memcpy(reg, other.reg, sizeof(reg));
    std::memcpy(RegisterTable, other.RegisterTable, sizeof(RegisterTable));
    std::memcpy(IntegerRS, other.IntegerRS, sizeof(IntegerRS));
    std::memcpy(StoreRS, other.StoreRS, sizeof(StoreRS));
    std::memcpy(LoadRS, other.LoadRS, sizeof(LoadRS));
    ROBModule = other.ROBModule;
    ALUModule = other.ALUModule;
    InstructMem = other.InstructMem;
    DataMem = other.DataMem;
  }
  systemState &operator=(const systemState &other) {
    if (this == &other)
      return *this;
    std::memcpy(reg, other.reg, sizeof(reg));
    std::memcpy(RegisterTable, other.RegisterTable, sizeof(RegisterTable));
    std::memcpy(IntegerRS, other.IntegerRS, sizeof(IntegerRS));
    std::memcpy(StoreRS, other.StoreRS, sizeof(StoreRS));
    std::memcpy(LoadRS, other.LoadRS, sizeof(LoadRS));
    ROBModule = other.ROBModule;
    ALUModule = other.ALUModule;
    InstructMem = other.InstructMem;
    DataMem = other.DataMem;
    return *this;
  }
};

class CPU {
private:
  systemState curCPUstate;
  systemState nextCPUstate;
  uint32_t programCounter;
  bool PCWriteEnable;

  friend void run_ALU_tomasulo_tests();

public:
  CPU(Memory mem);
  // functions about issue
  int issue();
  bool issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk);

  // functions about execution
  static Op decodeOp(Instruct inst);
  void execute();
  void load_n_bytes(int rd, int rs1, int imm, int n, bool isSigned);
  void store_n_bytes(int rs1, int rs2, int imm, int n);
  void apply_R_operation(Instruct inst);
  void apply_I_operation(Instruct inst);
  void apply_Istar_operation(Instruct inst);
  void apply_S_operation(Instruct inst);
  void apply_B_operation(Instruct inst);
  void apply_J_operation(Instruct inst);
  void apply_U_operation(Instruct inst);

  // functions about write result
  void writeBack();

  // functions about commit
  void commit();

  void run();
};

#endif // CPU_HPP