#pragma once
#ifndef COMMON_HPP
#define COMMON_HPP
#include <cstdint>
constexpr int INTEGERRS_CAP = 8;
constexpr int STORERS_CAP = 4;
constexpr int LOADRS_CAP = 4;
constexpr int BRANCHRS_CAP = 4;
constexpr int LSQ_CAP = 64;
constexpr int ROB_CAP = 64;
constexpr int INQ_CAP = 8;
constexpr int REGISTER_CAP = 32;
constexpr int FLUSHARBITER_CAP = 4;
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
  EQ,
  GE,
  GEU,
  LT,
  LTU,
  NE,
  Load,
  Store,
  JALR,
  OP_INVALID,
};
constexpr bool isMemoryOp(Operation op) {
  return op == Operation::Load || op == Operation::Store;
}
constexpr bool isControlOp(Operation op) { return op == Operation::JALR; }
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
  uint32_t pc = 0;
  bool isHalt = false;
};

struct IssueResult {
  bool valid = false;
  int rd = 0;
  int tag = -1;
};

struct ExecuteResult {
  int32_t value;
  int robTag;
  bool isAddress;
  bool isControl;
};

struct BranchResult {
  int pcResult;
  int robTag;
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

struct RATWritePort {
  bool valid = false;
  uint32_t reg = 0;
  int32_t value = 0;
};

struct SquashInfo {
  bool needSquash = false;
  int SquashTag = -1;
  uint32_t SquashPC = 0;
};

struct CDBOutput {
  ExecuteResult result;
  bool valid;
  bool aluGranted;
  bool lsqGranted;
};
#endif // COMMON_HPP