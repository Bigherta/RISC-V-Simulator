#pragma once
#ifndef CPU_HPP
#define CPU_HPP
#include "../include/Memory.hpp"
#include "Register.hpp"
#include "common.hpp"
#include <cstdint>
class CPU {
private:
  Register reg[32];
  Memory InstructMem, DataMem;
  uint32_t programCounter;

public:
  CPU(Memory mem);
  void calculate(int rd, int32_t op1, int32_t op2, Op op);
  void load_n_bytes(int rd, int rs1, int imm, int n, bool isSigned);
  void store_n_bytes(int rs1, int rs2, int imm, int n);
  void apply_operation(Instruct inst);
  void apply_R_operation(Instruct inst);
  void apply_I_operation(Instruct inst);
  void apply_Istar_operation(Instruct inst);
  void apply_S_operation(Instruct inst);
  void apply_B_operation(Instruct inst);
  void apply_J_operation(Instruct inst);
  void apply_U_operation(Instruct inst);
  void run();
};

inline CPU::CPU(Memory mem)
    : InstructMem(mem), DataMem(mem), programCounter(0x0000) {}
#endif // CPU_HPP