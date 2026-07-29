#include "../include/CPU.hpp"
#include "../include/Decoder.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>

CPU::CPU(Memory mem)
    : curCPUstate(mem), nextCPUstate(mem), programCounter(0x0000),
      PCWriteEnable(true) {}

bool CPU::issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk) {
  bool isFull = true;
  for (auto IntegerRS : curCPUstate.IntegerRS) {
    if (IntegerRS.free) {
      isFull = false;
      break;
    }
  }
  if (isFull || curCPUstate.CPUROB.isFull()) {
    return false;
  }
  ReservationStation IntegerRS;
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto destination = inst.rd;
  bool rs1_ready = (curCPUstate.RegisterTable[regNum1] == -1);
  bool rs2_ready = !has_rs2 || (curCPUstate.RegisterTable[regNum2] == -1);
  if (rs1_ready && rs2_ready) {
    IntegerRS.vj = curCPUstate.reg[regNum1].read();
    if (imm_as_vk) {
      IntegerRS.vk = inst.imm;
    } else {
      IntegerRS.vk = curCPUstate.reg[regNum2].read();
    }
  } else {
    IntegerRS.qj = curCPUstate.RegisterTable[regNum1];
    if (imm_as_vk) {
      IntegerRS.vk = inst.imm;
    } else {
      IntegerRS.qk = curCPUstate.RegisterTable[regNum2];
    }
  }
  ROBEntry newROB(REGISTER);
  newROB.dest = destination;
  newROB.tag = nextCPUstate.CPUROB.push(newROB);
  nextCPUstate.RegisterTable[destination] = newROB.tag;
  IntegerRS.ROB_dest = newROB.tag;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (curCPUstate.IntegerRS[i].free) {
      nextCPUstate.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return true;
}

int CPU::issue() {
  auto first_byte =
      static_cast<uint32_t>(curCPUstate.InstructMem.read_data(programCounter));
  auto second_byte = static_cast<uint32_t>(
      curCPUstate.InstructMem.read_data(programCounter + 1));
  auto third_byte = static_cast<uint32_t>(
      curCPUstate.InstructMem.read_data(programCounter + 2));
  auto fourth_byte = static_cast<uint32_t>(
      curCPUstate.InstructMem.read_data(programCounter + 3));
  auto raw_inst = first_byte | (second_byte << 8) | (third_byte << 16) |
                  (fourth_byte << 24);
  auto inst = Decoder::decode(raw_inst);
  if (raw_inst == 0x0ff00513) {
    return inst.rd;
  }
  switch (inst.type) {
  case R: {
    if (!issue_IntegerRS(inst, true, false)) {
      return -1;
    }
    break;
  }
  case I: {
    if (inst.opcode == 0b0010011) {
      if (!issue_IntegerRS(inst, false, true)) {
        return -1;
      }
    }
    break;
  }
  case Istar: {
    if (!issue_IntegerRS(inst, false, true)) {
      return -1;
    }
    break;
  }
  case S:
  case B:
  case U:
  case J:
  case RV_INVALID:
    break;
  }
  return -1;
}
int32_t CPU::ALU(int32_t op1, int32_t op2, Op op) {
  switch (op) {
  case ADD:
    return op1 + op2;
  case SUB:
    return op1 - op2;
  case XOR:
    return op1 ^ op2;
  case OR:
    return op1 | op2;
  case AND:
    return op1 & op2;
  case SL:
    return static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
  case SRL:
    return static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
  case SRA:
    return op1 >> (op2 & 0x1F);
  case SLT:
    return op1 < op2 ? 1 : 0;
  case SLTU:
    return static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
  default:
    return 0;
  }
}

Op CPU::decodeOp(Instruct inst) {
  if (inst.type == R) {
    int link_funct = (inst.funct3 << 7) | inst.funct7;
    switch (link_funct) {
    case 0b0000000000:
      return ADD;
    case 0b0000100000:
      return SUB;
    case 0b0010000000:
      return SL;
    case 0b0100000000:
      return SLT;
    case 0b0110000000:
      return SLTU;
    case 0b1000000000:
      return XOR;
    case 0b1010000000:
      return SRL;
    case 0b1010100000:
      return SRA;
    case 0b1100000000:
      return OR;
    case 0b1110000000:
      return AND;
    default:
      return OP_INVALID;
    }
  }
  if (inst.type == I) {
    switch (inst.funct3) {
    case 0b000:
      return ADD;
    case 0b010:
      return SLT;
    case 0b011:
      return SLTU;
    case 0b100:
      return XOR;
    case 0b110:
      return OR;
    case 0b111:
      return AND;
    default:
      return OP_INVALID;
    }
  }
  if (inst.type == Istar) {
    if (inst.funct3 == 1)
      return SL;
    if (inst.funct3 == 5)
      return (inst.funct7 == 0) ? SRL : SRA;
    return OP_INVALID;
  }
  return OP_INVALID;
}

void CPU::load_n_bytes(int rd, int rs1, int imm, int n, bool isSigned) {
  int32_t result = 0;
  uint32_t start_address = curCPUstate.reg[rs1].read() + imm;
  for (int i = 0; i < n; i++) {
    auto byte_data =
        static_cast<uint32_t>(curCPUstate.DataMem.read_data(start_address + i));
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
  curCPUstate.reg[rd].write(result);
}

void CPU::store_n_bytes(int rs1, int rs2, int imm, int n) {
  uint32_t start_address = curCPUstate.reg[rs1].read() + imm;
  auto data = curCPUstate.reg[rs2].read();
  //   std::cout << "  store " << n << "B x" << std::dec << rs2 << " -> " <<
  //   std::hex
  //             << start_address << std::dec << std::endl;
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(data >> (i << 3));
    curCPUstate.DataMem.write_data(start_address + i, byte_data);
  }
}

void CPU::apply_I_operation(Instruct inst) {
  auto opNum1 = curCPUstate.reg[inst.rs1].read();
  if (inst.opcode == 0b0010011) {
    switch (inst.funct3) {
    case 0b000:
      //       std::cout << "  addi x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, ADD));
      break; // addi
    case 0b010:
      //       std::cout << "  slti x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, SLT));
      break; // slti
    case 0b011:
      //       std::cout << "  sltiu x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, SLTU));
      break; // sltiu
    case 0b100:
      //       std::cout << "  xori x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, XOR));
      break; // xori
    case 0b110:
      //       std::cout << "  ori x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1 << ", "
      //                 << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, OR));
      break; // ori
    case 0b111:
      //       std::cout << "  andi x" << std::dec << inst.rd << ", x" <<
      //       inst.rs1
      //                 << ", " << inst.imm << std::endl;
      curCPUstate.reg[inst.rd].write(ALU(opNum1, inst.imm, AND));
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
    curCPUstate.reg[inst.rd].write(programCounter + 4);
    programCounter =
        static_cast<uint32_t>(curCPUstate.reg[inst.rs1].read() + inst.imm);
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
  auto opNum1 = curCPUstate.reg[inst.rs1].read();
  auto opNum2 = curCPUstate.reg[inst.rs2].read();
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
  curCPUstate.reg[inst.rd].write(programCounter + 4);
  programCounter += inst.imm;
}

void CPU::apply_U_operation(Instruct inst) {
  switch (inst.opcode) {
  case 0b0010111:
    //     std::cout << "  auipc x" << std::dec << inst.rd << ", 0x" << std::hex
    //               << inst.imm << std::dec << std::endl;
    curCPUstate.reg[inst.rd].write(programCounter + inst.imm);
    break;
  case 0b0110111:
    //     std::cout << "  lui x" << std::dec << inst.rd << ", 0x" << std::hex
    //               << inst.imm << std::dec << std::endl;
    curCPUstate.reg[inst.rd].write(inst.imm);
    break;
  }
}

void CPU::execute() {
  int Execute_IntegerRS_index;
  ReservationStation Execute_IntegerRS(OP_INVALID);
  Execute_IntegerRS.ROB_dest = ~0u >> 1;
  for (int i = 0; i < INTEGERRS_CAP; ++i) {
    auto rs = curCPUstate.IntegerRS[i];
    if (rs.qj == -1 && rs.qk == -1) {
      if (rs.ROB_dest < Execute_IntegerRS.ROB_dest) {
        Execute_IntegerRS = rs;
        Execute_IntegerRS_index = i;
      }
    }
  }
  if (Execute_IntegerRS.op == OP_INVALID) {
    return;
  }
  auto result =
      ALU(Execute_IntegerRS.vj, Execute_IntegerRS.vk, Execute_IntegerRS.op);
  nextCPUstate.IntegerRS[Execute_IntegerRS_index].free = true;
  nextCPUstate.commonDataBus = {true, result, Execute_IntegerRS.ROB_dest};
}

void CPU::writeBack() {
  if (!curCPUstate.commonDataBus.is_valid)
    return;
  auto tag = curCPUstate.commonDataBus.rob_mark;
  auto value = curCPUstate.commonDataBus.value;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (curCPUstate.IntegerRS[i].qj == tag) {
      nextCPUstate.IntegerRS[i].vj = value;
      nextCPUstate.IntegerRS[i].qj = -1;
    }
    if (curCPUstate.IntegerRS[i].qk == tag) {
      nextCPUstate.IntegerRS[i].vk = value;
      nextCPUstate.IntegerRS[i].qk = -1;
    }
  }
  nextCPUstate.CPUROB.writeROB(value, curCPUstate.CPUROB.getIndex(tag), Ready);
}

void CPU::commit() {
  if (nextCPUstate.CPUROB.peek().state != Ready)
    return;
  auto rob_entry = nextCPUstate.CPUROB.pop();
  nextCPUstate.reg[rob_entry.dest].write(rob_entry.value);
}

void CPU::run() {
  bool finish = false;
  uint64_t clock = 0;
  while (!finish) {
    nextCPUstate = curCPUstate;
    commit();
    writeBack();
    execute();
    auto result = issue();
    finish = result != -1;
    auto old_pc = programCounter;
    if (programCounter == old_pc && PCWriteEnable)
      programCounter += 4;
    curCPUstate.reg[0].write(0);
    if (finish) {
      std::cout << std::dec << (curCPUstate.reg[result].read() & 0xFF)
                << std::endl;
      std::cout << "Clock cycles: " << clock << std::endl;
    }
    curCPUstate = nextCPUstate;
    ++clock;
  }
}