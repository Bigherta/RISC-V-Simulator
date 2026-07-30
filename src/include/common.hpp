#pragma once
#ifndef COMMON_HPP
#define COMMON_HPP
#include <cstdint>
enum Op {
  ADD,
  SUB,
  AND,
  OR,
  XOR,
  SL,
  SRL,
  SRA,
  SLT,
  SLTU,
  OP_INVALID,
};

enum RISC_V {
  R,
  I,
  Istar,
  S,
  B,
  U,
  J,
  RV_INVALID,
};

struct Instruct {
  RISC_V type = RV_INVALID;
  int opcode = 0;
  int funct3 = 0;
  int funct7 = 0;
  int rd = 0;
  int rs1 = 0;
  int rs2 = 0;
  int32_t imm = 0;
};

struct ExecuteResult {
  int32_t value;
  int robTag;
};
#endif // COMMON_HPP