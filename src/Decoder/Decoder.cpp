#include "../include/Decoder.hpp"

Instruct Decoder::decode(int32_t raw_inst) {
  Instruct inst;
  uint32_t raw = static_cast<uint32_t>(raw_inst);
  auto opcode = raw & 0x7F;
  inst.opcode = opcode;
  inst.funct3 = (raw >> 12) & 0x7;
  inst.rd = (raw >> 7) & 0x1F;
  inst.rs1 = (raw >> 15) & 0x1F;
  inst.rs2 = (raw >> 20) & 0x1F;

  auto getImm = [&](RISC_V type) -> int32_t {
    switch (type) {
    case Istar:
      return static_cast<int32_t>((raw >> 20) & 0x1F);
    case I:
      return signExtend(static_cast<int32_t>((raw >> 20) & 0xFFF), 12);
    case S:
      return signExtend(
          static_cast<int32_t>(((raw >> 7) & 0x1F) | ((raw >> 25) << 5)), 12);
    case B:
      return signExtend(static_cast<int32_t>(
                            ((raw >> 31) << 12) | (((raw >> 25) & 0x3F) << 5) |
                            ((inst.rd & 1) << 11) | ((inst.rd >> 1) << 1)),
                        13);
    case J:
      return signExtend(static_cast<int32_t>(((raw >> 31) << 20) |
                                             (((raw >> 21) & 0x3FF) << 1) |
                                             (((raw << 11) >> 31) << 11) |
                                             (((raw << 12) >> 24) << 12)),
                        21);
    case U:
      return static_cast<int32_t>(raw_inst & 0xFFFFF000);
    default:
      return 0;
    }
  };

  switch (opcode) {
  case 0b0110011: {
    inst.type = R;
    inst.funct7 = (raw >> 25) & 0x7F;
    break;
  }
  case 0b0010011: {
    inst.funct7 = (raw >> 25) & 0x7F;
    auto link_funct = (inst.funct3 << 7) | inst.funct7;
    if (link_funct == 0b0010000000 || link_funct == 0b1010000000 ||
        link_funct == 0b1010100000) {
      inst.type = Istar;
    } else {
      inst.type = I;
    }
    inst.imm = getImm(inst.type);
    break;
  }
  case 0b0000011:
  case 0b1100111: {
    inst.type = I;
    inst.imm = getImm(I);
    break;
  }
  case 0b0100011: {
    inst.type = S;
    inst.imm = getImm(S);
    break;
  }
  case 0b1100011: {
    inst.type = B;
    inst.imm = getImm(B);
    break;
  }
  case 0b1101111: {
    inst.type = J;
    inst.imm = getImm(J);
    break;
  }
  case 0b0010111:
  case 0b0110111: {
    inst.type = U;
    inst.imm = getImm(U);
    break;
  }
  default:
    inst.type = INVALID;
    break;
  }
  return inst;
}