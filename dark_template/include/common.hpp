#pragma once
#ifndef DARK_COMMON_HPP
#define DARK_COMMON_HPP
#include "helper/concept.h"
#include "helper/register.h"
#include "helper/wire.h"
#include <array>
#include <cstdint>

namespace dark {

static inline constexpr int INTEGERRS_CAP = 8;
static inline constexpr int STORERS_CAP = 4;
static inline constexpr int LOADRS_CAP = 4;
static inline constexpr int BRANCHRS_CAP = 4;
static inline constexpr int LSQ_CAP = 64;
static inline constexpr int ROB_CAP = 64;
static inline constexpr int INQ_CAP = 8;
static inline constexpr int DECODER_CAP = 16;
static inline constexpr int REGISTER_CAP = 32;
static inline constexpr int FLUSHARBITER_CAP = 4;
static inline constexpr int ALU_CAP = 4;
static inline constexpr int BRU_CAP = 4;
static inline constexpr int BHT_CAP = 1 << 12;
static inline constexpr int SELECTOR_CAP = 1 << 12;
static inline constexpr int HISTORY_BIT = 12;
static inline constexpr int HISTORY_MASK = (1 << HISTORY_BIT) - 1;
static inline constexpr int BTB_CAP = 4096;
static inline constexpr int RAS_CAP = 128;
enum class Operation : max_size_t {
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
inline constexpr bool isMemoryOp(Operation op) {
  return op == Operation::Load || op == Operation::Store;
}
inline constexpr bool isControlOp(Operation op) { return op == Operation::JALR; }
enum class RISC_V : max_size_t {
  R,
  I,
  Istar,
  S,
  B,
  U,
  J,
  RV_INVALID,
};

enum class ROBType : max_size_t {
  REGISTER,
  BRANCH,
  STORE,
  LINK,
};

struct SquashReq {
  Wire<1> valid;
  Wire<31> tag;
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

inline Operation decodeOp(const Instruct &inst) { // shared by DEC/RAT/... (moved from src/Decoder)
	if (inst.type == RISC_V::R) {
		int link_funct = (inst.funct3 << 7) | inst.funct7;
		switch (link_funct) {
		case 0b0000000000:
			return Operation::ADD;
		case 0b0000100000:
			return Operation::SUB;
		case 0b0010000000:
			return Operation::SL;
		case 0b0100000000:
			return Operation::SLT;
		case 0b0110000000:
			return Operation::SLTU;
		case 0b1000000000:
			return Operation::XOR;
		case 0b1010000000:
			return Operation::SRL;
		case 0b1010100000:
			return Operation::SRA;
		case 0b1100000000:
			return Operation::OR;
		case 0b1110000000:
			return Operation::AND;
		default:
			return Operation::OP_INVALID;
		}
	}
	if (inst.type == RISC_V::I) {
		if (inst.opcode == 0b0000011)
			return Operation::Load;
		if (inst.opcode == 0b1100111)
			return Operation::JALR;
		switch (inst.funct3) {
		case 0b000:
			return Operation::ADD;
		case 0b010:
			return Operation::SLT;
		case 0b011:
			return Operation::SLTU;
		case 0b100:
			return Operation::XOR;
		case 0b110:
			return Operation::OR;
		case 0b111:
			return Operation::AND;
		default:
			return Operation::OP_INVALID;
		}
	}
	if (inst.type == RISC_V::Istar) {
		if (inst.funct3 == 1)
			return Operation::SL;
		if (inst.funct3 == 5)
			return (inst.funct7 == 0) ? Operation::SRL : Operation::SRA;
		return Operation::OP_INVALID;
	}
	if (inst.type == RISC_V::S) {
		return Operation::Store;
	}
	if (inst.type == RISC_V::B) {
		switch (inst.funct3) {
		case 0b000:
			return Operation::EQ;
		case 0b001:
			return Operation::NE;
		case 0b100:
			return Operation::LT;
		case 0b101:
			return Operation::GE;
		case 0b110:
			return Operation::LTU;
		case 0b111:
			return Operation::GEU;
		default:
			return Operation::OP_INVALID;
		}
	}
	if (inst.type == RISC_V::U) {
		if (inst.opcode == 0b0010111)
			return Operation::AUIPC;
		if (inst.opcode == 0b0110111)
			return Operation::LUI;
		return Operation::OP_INVALID;
	}
	if (inst.type == RISC_V::J) {
		return Operation::JALR;
	}
	return Operation::OP_INVALID;
}

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
  int pcFrom;
  int pcResult;
  int robTag;
};

struct CDBInfo {
	uint8_t index;
	bool busy;
	ExecuteResult result;
};

struct EmptyOutput { // stateless combined-only modules (EXEC_SEL / CDB_Arb)
	std::array<Register<1>, 1> dummy; // sync-reflect needs >= 1 member
};

struct CDBIn { // result broadcast (bound to CDB_Arb combined getters)
	Wire<1> valid;
	Wire<31> tag;
	Wire<32> value;
	Wire<1> is_address;
	Wire<1> is_control;
	Wire<32> rob_value; // control (JALR): LINK value from ROB (wired via lambda)
};

struct ExecAccept { // EXEC_SEL arbitration result (bound to its combined getters)
	Wire<1> valid;
	Wire<3> src; // 0=int 1=load 2=store
	Wire<6> index; // selected slot (in source RS)
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
};

struct BranchPredictorCkpt {
  int top = 0;
  uint16_t GHR = 0;
};

} // namespace dark
#endif // DARK_COMMON_HPP
