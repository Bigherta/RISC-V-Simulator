#pragma once
#ifndef COMMON_HPP
#define COMMON_HPP
#include <cstdint>
constexpr int INTEGERRS_CAP = 8;
constexpr int STORERS_CAP = 4;
constexpr int LOADRS_CAP = 4;
constexpr int LSQ_CAP = 64;
constexpr int ROB_CAP = 64;
enum class Operation {
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
  AUIPC,
  LUI,
  Load,
  Store,
  OP_INVALID,
};
constexpr bool isMemoryOp(Operation op) {
  return op == Operation::Load || op == Operation::Store;
}
enum class RISC_V {
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
  RISC_V type = RISC_V::RV_INVALID;
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
  bool isAddress;
};

struct CDBInfo {
  uint8_t index;
  bool busy;
  ExecuteResult result;
};

struct MemRequest {
  Operation op;
  int remainCycle = 3;
  int32_t value;
  uint32_t address;
  bool isSigned;
  int n_bytes;
  int ROBTag;
};

#endif // COMMON_HPP