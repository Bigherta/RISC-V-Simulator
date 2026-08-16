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
constexpr int IMEM_CAP = 4;
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
  bool btbHit = false;       // BTB 命中（分支类型信息来自 BTB）
  bool unconditional = false;
  bool isCall = false;       // JAL rd==1
  bool isRet = false;        // JALR x0, 0(x1)
};

struct BTBEntry {
  uint32_t actualPC;
  uint32_t target;
  bool valid;
  bool unconditional;
  bool isCall = false;
  bool isRet = false;
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
enum class RSType { Integer, Branch, Load, StoreAddr };
struct DispatchInfo {
  bool valid = false;
  int rsIndex = -1;
  int robIndex = -1;
  uint64_t robSeq = 0;
  RSType rsType = RSType::Integer;
};
struct DispatchBus {
  DispatchInfo alu, agu, bru;
};
class ROB;
class PRF;
struct CDBBus {
  bool broadcastValid = false;
  int broadcastValue = 0;
  bool lsqSetCDB = false;
  int robIndex = -1;
  uint64_t robSeq = 0;
  // read() 边组合求值（同 CDBArbiter 模式）：广播判定与载荷一次打包，
  // 消费者只"应用"不"重算"（定义见 Arbiter.cpp）。
  static CDBBus build(const CDBOutput &cdbOut, const ROB &ROBModule,
                      const PRF &PRFModule, const SquashInfo &squashDetect);
};
struct BranchPredictor;
struct FetchDecision {
  bool valid = false;
  uint32_t pc = 0;
  int32_t predictedPC = 0;
  bool isCall = false;
  bool isRet = false;
  bool shift = false;
  bool shiftValue = false;
  BranchPredictorSnapshot ckpt;
  // read() 边组合求值：取指条件（squash/halt/FQ 满/窗口满）+ predict（快照
  // BP）+ 预测拍快照（定义见 BranchPredictor.cpp）。
  static FetchDecision build(const BranchPredictor &bp, uint32_t pc,
                             const SquashInfo &squash, bool haltFetched,
                             bool fqFull, bool imemReqFull);
};
#endif // COMMON_HPP