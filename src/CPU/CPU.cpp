#include "../include/CPU.hpp"
#include "../include/Decoder.hpp"
#include <cstdint>
#include <iostream>

void CPU::calculate(int rd, int32_t op1, int32_t op2, Op op) {
  switch (op) {
  case ADD:
    reg[rd].write(op1 + op2);
    break;
  case SUB:
    reg[rd].write(op1 - op2);
    break;
  case XOR:
    reg[rd].write(op1 ^ op2);
    break;
  case OR:
    reg[rd].write(op1 | op2);
    break;
  case AND:
    reg[rd].write(op1 & op2);
    break;
  case SL:
    reg[rd].write(
        static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F)));
    break;
  case SRL:
    reg[rd].write(
        static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F)));
    break;
  case SRA:
    reg[rd].write(op1 >> (op2 & 0x1F));
    break;
  case SLT:
    reg[rd].write(op1 < op2 ? 1 : 0);
    break;
  case SLTU:
    reg[rd].write(static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1
                                                                          : 0);
    break;
  }
}

void CPU::load_n_bytes(int rd, int rs1, int imm, int n, bool isSigned) {
  int32_t result = 0;
  uint32_t start_address = reg[rs1].read() + imm;
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint32_t>(mem.read_data(start_address + i));
    result |= (byte_data << (i << 3));
    if (i == n - 1 && n < 4 && isSigned) {
      if (result & (1 << ((n << 3) - 1))) {
        auto mask = ~((1 << (n << 3)) - 1);
        result |= mask;
      }
    }
  }
  //   std::cout << "  load " << n << "B x" << std::dec << rd << " <- " <<
  //   std::hex
  //             << start_address << std::dec << std::endl;
  reg[rd].write(result);
}

void CPU::store_n_bytes(int rs1, int rs2, int imm, int n) {
  uint32_t start_address = reg[rs1].read() + imm;
  auto data = reg[rs2].read();
  //   std::cout << "  store " << n << "B x" << std::dec << rs2 << " -> " <<
  //   std::hex
  //             << start_address << std::dec << std::endl;
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(data >> (i << 3));
    mem.write_data(start_address + i, byte_data);
  }
}

void CPU::apply_R_operation(Instruct inst) {
  auto link_funct = (inst.funct3 << 7) | inst.funct7;
  auto opNum1 = reg[inst.rs1].read();
  auto opNum2 = reg[inst.rs2].read();
  switch (link_funct) {
  case 0b0000000000:
    //     std::cout << "  add x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, ADD);
    break;
  case 0b0000100000:
    //     std::cout << "  sub x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SUB);
    break;
  case 0b0010000000:
    //     std::cout << "  sll x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SL);
    break;
  case 0b0100000000:
    //     std::cout << "  slt x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SLT);
    break;
  case 0b0110000000:
    //     std::cout << "  sltu x" << std::dec << inst.rd << ", x" << inst.rs1
    //     << ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SLTU);
    break;
  case 0b1000000000:
    //     std::cout << "  xor x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, XOR);
    break;
  case 0b1010000000:
    //     std::cout << "  srl x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SRL);
    break;
  case 0b1010100000:
    //     std::cout << "  sra x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, SRA);
    break;
  case 0b1100000000:
    //     std::cout << "  or x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, OR);
    break;
  case 0b1110000000:
    //     std::cout << "  and x" << std::dec << inst.rd << ", x" << inst.rs1 <<
    //     ", x"
    //               << inst.rs2 << std::endl;
    calculate(inst.rd, opNum1, opNum2, AND);
    break;
  }
}

void CPU::apply_I_operation(Instruct inst) {
  auto opNum1 = reg[inst.rs1].read();
  if (inst.opcode == 0b0010011) {
    switch (inst.funct3) {
    case 0b000:
      //       std::cout << "  addi x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, ADD);
      break; // addi
    case 0b010:
      //       std::cout << "  slti x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, SLT);
      break; // slti
    case 0b011:
      //       std::cout << "  sltiu x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, SLTU);
      break; // sltiu
    case 0b100:
      //       std::cout << "  xori x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, XOR);
      break; // xori
    case 0b110:
      //       std::cout << "  ori x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1 << ", "
      //                 << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, OR);
      break; // ori
    case 0b111:
      //       std::cout << "  andi x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, AND);
      break; // andi
    }
  } else if (inst.opcode == 0b0000011) {
    switch (inst.funct3) {
    case 0b000:
      //       std::cout << "  lb x" << std::dec << inst.rd << ", " << inst.imm
      //       << "(x"
      //                 << inst.rs1 << ")" << std::endl;
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 1, true);
      break; // lb
    case 0b001:
      //       std::cout << "  lh x" << std::dec << inst.rd << ", " << inst.imm
      //       << "(x"
      //                 << inst.rs1 << ")" << std::endl;
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 2, true);
      break; // lh
    case 0b010:
      //       std::cout << "  lw x" << std::dec << inst.rd << ", " << inst.imm
      //       << "(x"
      //                 << inst.rs1 << ")" << std::endl;
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 4, false);
      break; // lw
    case 0b100:
      //       std::cout << "  lbu x" << std::dec << inst.rd << ", " << inst.imm
      //       << "(x"
      //                 << inst.rs1 << ")" << std::endl;
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 1, false);
      break; // lbu
    case 0b101:
      //       std::cout << "  lhu x" << std::dec << inst.rd << ", " << inst.imm
      //       << "(x"
      //                 << inst.rs1 << ")" << std::endl;
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 2, false);
      break; // lhu
    }
  } else if (inst.opcode == 0b1100111) {
    //     std::cout << "  jalr x" << std::dec << inst.rd << ", x" << inst.rs1
    //     << ", "
    //               << inst.imm << std::endl;
    reg[inst.rd].write(programCounter + 4);
    programCounter = static_cast<uint32_t>(reg[inst.rs1].read() + inst.imm);
  }
}

void CPU::apply_Istar_operation(Instruct inst) {
  auto opNum1 = reg[inst.rs1].read();
  switch (inst.funct3) {
  case 1: // slli
    //     std::cout << "  slli x" << std::dec << inst.rd << ", x" << inst.rs1
    //     << ", "
    //               << inst.imm << std::endl;
    calculate(inst.rd, opNum1, inst.imm, SL);
    break;
  case 5: // srli (funct7=0000000) / srai (funct7=0100000)
    if (inst.funct7 == 0) {
      //       std::cout << "  srli x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, SRL);
    } else {
      //       std::cout << "  srai x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      calculate(inst.rd, opNum1, inst.imm, SRA);
    }
    break;
  }
}

void CPU::apply_S_operation(Instruct inst) {
  switch (inst.funct3) {
  case 0b000:
    //     std::cout << "  sb x" << std::dec << inst.rs2 << ", " << inst.imm <<
    //     "(x"
    //               << inst.rs1 << ")" << std::endl;
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 1);
    break; // sb
  case 0b001:
    //     std::cout << "  sh x" << std::dec << inst.rs2 << ", " << inst.imm <<
    //     "(x"
    //               << inst.rs1 << ")" << std::endl;
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 2);
    break; // sh
  case 0b010:
    //     std::cout << "  sw x" << std::dec << inst.rs2 << ", " << inst.imm <<
    //     "(x"
    //               << inst.rs1 << ")" << std::endl;
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 4);
    break; // sw
  }
}

void CPU::apply_B_operation(Instruct inst) {
  auto opNum1 = reg[inst.rs1].read();
  auto opNum2 = reg[inst.rs2].read();
  auto offset = inst.imm;
  switch (inst.funct3) {
  case 0b000:
    //     std::cout << "  beq x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (opNum1 == opNum2) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // beq
  case 0b001:
    //     std::cout << "  bne x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (opNum1 != opNum2) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // bne
  case 0b100:
    //     std::cout << "  blt x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (opNum1 < opNum2) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // blt
  case 0b101:
    //     std::cout << "  bge x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (opNum1 >= opNum2) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // bge
  case 0b110:
    //     std::cout << "  bltu x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (static_cast<uint32_t>(opNum1) < static_cast<uint32_t>(opNum2)) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // bltu
  case 0b111:
    //     std::cout << "  bgeu x" << std::dec << inst.rs1 << ", x" << inst.rs2
    //     << ", "
    //               << offset;
    if (static_cast<uint32_t>(opNum1) >= static_cast<uint32_t>(opNum2)) {
      //       std::cout << " [taken]" << std::endl;
      programCounter += offset;
    } else {
      //       std::cout << " [not taken]" << std::endl;
    }
    break; // bgeu
  }
}

void CPU::apply_J_operation(Instruct inst) {
  //   std::cout << "  jal x" << std::dec << inst.rd << ", " << inst.imm
  //             << std::endl;
  reg[inst.rd].write(programCounter + 4);
  programCounter += inst.imm;
}

void CPU::apply_U_operation(Instruct inst) {
  switch (inst.opcode) {
  case 0b0010111:
    //     std::cout << "  auipc x" << std::dec << inst.rd << ", 0x" << std::hex
    //               << inst.imm << std::dec << std::endl;
    reg[inst.rd].write(programCounter + inst.imm);
    break;
  case 0b0110111:
    //     std::cout << "  lui x" << std::dec << inst.rd << ", 0x" << std::hex
    //               << inst.imm << std::dec << std::endl;
    reg[inst.rd].write(inst.imm);
    break;
  }
}

void CPU::apply_operation(Instruct inst) {
  switch (inst.type) {
  case R:
    apply_R_operation(inst);
    break;
  case I:
    apply_I_operation(inst);
    break;
  case Istar:
    apply_Istar_operation(inst);
    break;
  case S:
    apply_S_operation(inst);
    break;
  case B:
    apply_B_operation(inst);
    break;
  case U:
    apply_U_operation(inst);
    break;
  case J:
    apply_J_operation(inst);
    break;
  case INVALID:
    //     std::cout << "  <invalid instruction>" << std::endl;
    break;
  }
}

void CPU::run() {
  int32_t raw_inst = 0;
  while (raw_inst != 0x0ff00513) {
    auto first_byte = static_cast<uint32_t>(mem.read_data(programCounter));
    auto second_byte = static_cast<uint32_t>(mem.read_data(programCounter + 1));
    auto third_byte = static_cast<uint32_t>(mem.read_data(programCounter + 2));
    auto fourth_byte = static_cast<uint32_t>(mem.read_data(programCounter + 3));
    raw_inst = first_byte | (second_byte << 8) | (third_byte << 16) |
               (fourth_byte << 24);
    auto inst = Decoder::decode(raw_inst);
    if (raw_inst == 0x0ff00513) {
      break;
    }
    auto old_pc = programCounter;
    //     std::cout << "0x" << std::hex << programCounter << ": " << std::hex
    //               << raw_inst << std::dec << std::endl;
    apply_operation(inst);
    if (programCounter == old_pc)
      programCounter += 4;
    reg[0].write(0);
  }
  auto inst = Decoder::decode(raw_inst);
  std::cout << std::dec << (reg[inst.rd].read() & 0xFF);
}