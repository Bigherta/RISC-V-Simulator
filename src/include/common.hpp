#pragma once
#ifndef COMMON_HPP
#define COMMON_HPP
#include <cstdint>
using RobTag = uint8_t;
constexpr int INTEGERRS_CAP = 16;
constexpr int STORERS_CAP = 8;
constexpr int LOADRS_CAP = 4;
constexpr int BRANCHRS_CAP = 4;
constexpr int LQ_CAP = 16;
constexpr int SQ_CAP = 16;
constexpr int MEMQ_SCAN_WINDOW = 8;
constexpr uint8_t MEM_STORE_BIT = 0x40;
inline bool isStoreMem(uint8_t m) { return (m & MEM_STORE_BIT) != 0; }
inline uint8_t memSlot(uint8_t m) { return m & 0x3F; }
constexpr int ROB_CAP = 64;
constexpr int FQ_CAP = 8;
constexpr int IQ_CAP = 16;
constexpr int REGISTER_CAP = 32;
constexpr int FLUSHARBITER_CAP = 4;
constexpr int ALU_CAP = 4;
constexpr int AGU_CAP = 4;
constexpr int BRU_CAP = 4;
constexpr int PC_Direct_CAP = 1 << 12;
constexpr int BTB_CAP = 512;
constexpr int SELECTOR_CAP = 1 << 12;
constexpr int HISTORY_BIT = 12;
constexpr int HISTORY_MASK = (1 << HISTORY_BIT) - 1;
constexpr int LHT_CAP = 1 << 12;
constexpr int LOCAL_HISTORY_BIT = 8;
constexpr int LOCAL_PHT_CAP = 1 << LOCAL_HISTORY_BIT;
constexpr int RAS_CAP = 16;
constexpr int ALIGNQ_CAP = 32;
constexpr int PRF_CAP = 128;
constexpr int IMEM_CAP = 16;
constexpr int CKPT_CAP = 64;
constexpr int CACHE_BLOCK_CAP = 16;
constexpr int CACHE_CAP = 1024;
constexpr int REQUEST_CAP = 4;
enum class ValueState {
  NOTREADY,
  FETCHING,
  READY,
};

enum class SquashKind : uint8_t { None, Branch, LoadViolation };
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
  RobTag robTag;
  bool isControl;
};

struct AddressCalculateResult {
  int32_t value;
  RobTag robTag;
  uint8_t memIndex;
};

struct BranchResult {
  int pcFrom;
  int pcResult;
  RobTag robTag;
};

struct MemRequest {
  Operation op;
  int remainCycle = 3;
  int32_t value;
  uint32_t address;
  bool isSigned;
  int n_bytes;
  uint8_t robTag;
  uint8_t memIndex;
};

struct MemDispatchDecision {
  bool valid = false;
  MemRequest request{};
};

struct SquashInfo {
  bool needSquash = false;
  SquashKind kind = SquashKind::None;
  int SquashIndex = -1;
  uint8_t SquashTag = 0;
  uint32_t SquashPC = 0;
  uint8_t CkptId = 0;
};

struct CDBOutput {
  ArithmeticCalculateResult result;
  bool valid;
  bool aluGranted;
  bool memGranted;
  uint8_t memIndex = 0;
};

struct CDBBypassResult {
  bool valid = false;
  int32_t value = 0;
};

struct PredictInfo {
  bool taken;
  int32_t predictPC;
  bool btbHit = false;
  bool unconditional = false;
  bool isCall = false; // JAL rd==1
  bool isRet = false;  // JALR x0, 0(x1)
};

struct BTBEntry {
  uint32_t actualPC;
  uint32_t target;
  bool valid;
  bool unconditional;
  bool isCall = false;
  bool isRet = false;
};

struct BPUSnapshot {
  // SARAS: the checkpoint keeps GHR, AlignQueue head+tail, and RAS_top.
  // With RASEntry{retPC,times} the height != call/ret depth, so RAS_top
  // is checkpointed directly. All three are uint8_t — ring counters wrap
  // at 256 (8× ALIGNQ_CAP / 16× RAS_CAP, safe for in-flight <64).
  uint16_t GHR_snapshot;
  uint8_t alignHead;
  uint8_t alignTail;
  uint8_t RAS_top;
};

struct RATSnapshot {
  int RAT_snapshot[REGISTER_CAP];
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
  uint8_t ckptId = 0;
};
struct UopView {
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
  uint8_t ckptId = 0;
};
enum class RSType { Integer, Branch, Load, StoreAddr };
struct DispatchInfo {
  bool valid = false;
  int rsIndex = -1;
  uint8_t robTag = 0;
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
  RobTag robTag = 0;
  uint8_t memIndex = 0;
  static CDBBus build(const CDBOutput &cdbOut, const ROB &ROBModule,
                      const PRF &PRFModule, const SquashInfo &squashDetect);
};
struct BPU;
struct FetchDecision {
  bool valid = false;
  uint32_t pc = 0;
  int32_t predictedPC = 0;
  bool isCall = false;
  bool isRet = false;
  bool shift = false;
  bool shiftValue = false;
  uint8_t ckptId = 0;
  static FetchDecision build(const BPU &bp, uint32_t pc,
                             const SquashInfo &squash, bool haltFetched,
                             bool fqFull, bool imemReqFull);
};
struct LoadResponse {
  bool valid = false;
  uint8_t memIndex = 0;
  uint8_t robTag = 0;
  int32_t value = 0;
};

// IMEM -> ICache line-return bus: a full 16B cache line delivered as a
// fixed-width 4x32-bit word bundle (RTL-style data bus, not a pointer).
// word index 0..3 maps to byte offsets 0..15; the critical word for a fetch
// at pc is data[(pc >> 2) & 3].
struct LineReturn {
  bool valid = false;
  uint32_t lineAddr = 0;
  uint32_t data[4] = {0, 0, 0, 0};
};
#endif // COMMON_HPP