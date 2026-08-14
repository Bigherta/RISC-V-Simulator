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
constexpr int FQ_CAP = 8;
constexpr int IQ_CAP = 16;
constexpr int REGISTER_CAP = 32;
constexpr int FLUSHARBITER_CAP = 4;
constexpr int ALU_CAP = 4;
constexpr int AGU_CAP = 4;
constexpr int BRU_CAP = 4;
constexpr int PC_Direct_CAP = 1 << 12;
constexpr int SELECTOR_CAP = 1 << 12;
constexpr int HISTORY_BIT = 12;
constexpr int HISTORY_MASK = (1 << HISTORY_BIT) - 1;
constexpr int LHT_CAP = 1 << 12;
constexpr int LOCAL_HISTORY_BIT = 8;
constexpr int LOCAL_PHT_CAP = 1 << LOCAL_HISTORY_BIT;
constexpr int RAS_CAP = 128;
constexpr int PRF_CAP = 128;
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

struct ArithmeticCalculateResult {
  int32_t value;
  int robIndex;
  uint64_t robSeq;
  bool isControl;
};

struct AddressCalculateResult {
  int32_t value;
  int robIndex;
  uint64_t robSeq;
};

struct BranchResult {
  int pcFrom;
  int pcResult;
  int robIndex;
  uint64_t robSeq;
};

struct MemRequest {
  Operation op;
  int remainCycle = 3;
  int32_t value;
  uint32_t address;
  bool isSigned;
  int n_bytes;
  int robIndex;
  uint64_t robSeq;
};

struct SquashInfo {
  bool needSquash = false;
  int SquashIndex = -1;
  uint64_t SquashSeq = 0;
  uint32_t SquashPC = 0;
};

struct CDBOutput {
  ArithmeticCalculateResult result;
  bool valid;
  bool aluGranted;
  bool lsqGranted;
};

struct CDBBypassResult {
  bool valid = false;
  int32_t value = 0;
};

struct PredictInfo {
  bool taken;
  int32_t predictPC;
};

struct BTBEntry {
  uint32_t actualPC;
  uint32_t target;
  bool valid;
  bool unconditional;
};

struct BranchPredictorSnapshot {
  int top_snapshot;
  uint32_t RAS_snapshot[RAS_CAP];
  uint16_t GHR_snapshot;
};

struct RATSnapshot {
  int RAT_snapshot[REGISTER_CAP];
};

struct Checkpoint {
  RATSnapshot RATsnapshot;                
  BranchPredictorSnapshot BPsnapshot{};    
  uint32_t flHeadSeqCkpt = 0;              
};

struct Uop {
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
  bool allocDest = false;
  int32_t predictedPC = 0;
  BranchPredictorSnapshot BPSnapshot{};
};
#endif // COMMON_HPP