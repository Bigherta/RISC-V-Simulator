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
  if (isFull || curCPUstate.ROBModule.isFull()) {
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
    if (!rs1_ready)
      IntegerRS.qj = curCPUstate.RegisterTable[regNum1];
    else {
      IntegerRS.vj = curCPUstate.reg[regNum1].read();
    }
    if (imm_as_vk) {
      IntegerRS.vk = inst.imm;
    } else if (!rs2_ready && has_rs2) {
      IntegerRS.qk = curCPUstate.RegisterTable[regNum2];
    } else if (rs2_ready && has_rs2) {
      IntegerRS.vk = curCPUstate.reg[regNum2].read();
    }
  }
  ROBEntry newROB(REGISTER);
  newROB.dest = destination;
  newROB.tag = nextCPUstate.ROBModule.push(newROB);
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
  curCPUstate.reg[rd].write(result);
}

void CPU::store_n_bytes(int rs1, int rs2, int imm, int n) {
  uint32_t start_address = curCPUstate.reg[rs1].read() + imm;
  auto data = curCPUstate.reg[rs2].read();
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(data >> (i << 3));
    curCPUstate.DataMem.write_data(start_address + i, byte_data);
  }
}

void CPU::apply_I_operation(Instruct inst) {
  auto opNum1 = curCPUstate.reg[inst.rs1].read();
  if (inst.opcode == 0b0000011) {
    switch (inst.funct3) {
    case 0b000:
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 1, true);
      break;
    case 0b001:
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 2, true);
      break;
    case 0b010:
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 4, false);
      break;
    case 0b100:
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 1, false);
      break;
    case 0b101:
      load_n_bytes(inst.rd, inst.rs1, inst.imm, 2, false);
      break;
    }
  } else if (inst.opcode == 0b1100111) {
    curCPUstate.reg[inst.rd].write(programCounter + 4);
    programCounter =
        static_cast<uint32_t>(curCPUstate.reg[inst.rs1].read() + inst.imm);
  }
}

void CPU::apply_S_operation(Instruct inst) {
  switch (inst.funct3) {
  case 0b000:
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 1);
    break;
  case 0b001:
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 2);
    break;
  case 0b010:
    store_n_bytes(inst.rs1, inst.rs2, inst.imm, 4);
    break;
  }
}

void CPU::apply_B_operation(Instruct inst) {
  auto opNum1 = curCPUstate.reg[inst.rs1].read();
  auto opNum2 = curCPUstate.reg[inst.rs2].read();
  auto offset = inst.imm;
  switch (inst.funct3) {
  case 0b000:
    if (opNum1 == opNum2) {
      programCounter += offset;
    }
    break;
  case 0b001:
    if (opNum1 != opNum2) {
      programCounter += offset;
    }
    break;
  case 0b100:
    if (opNum1 < opNum2) {
      programCounter += offset;
    }
    break;
  case 0b101:
    if (opNum1 >= opNum2) {
      programCounter += offset;
    }
    break;
  case 0b110:
    if (static_cast<uint32_t>(opNum1) < static_cast<uint32_t>(opNum2)) {
      programCounter += offset;
    }
    break;
  case 0b111:
    if (static_cast<uint32_t>(opNum1) >= static_cast<uint32_t>(opNum2)) {
      programCounter += offset;
    }
    break;
  }
}

void CPU::apply_J_operation(Instruct inst) {
  curCPUstate.reg[inst.rd].write(programCounter + 4);
  programCounter += inst.imm;
}

void CPU::apply_U_operation(Instruct inst) {
  switch (inst.opcode) {
  case 0b0010111:
    curCPUstate.reg[inst.rd].write(programCounter + inst.imm);
    break;
  case 0b0110111:
    curCPUstate.reg[inst.rd].write(inst.imm);
    break;
  }
}

void CPU::execute() {
  if (nextCPUstate.ALUModule.isFull()) {
    return;
  }
  int Execute_IntegerRS_index;
  ReservationStation Execute_IntegerRS(OP_INVALID);
  Execute_IntegerRS.ROB_dest = ~0u >> 1;
  for (int i = 0; i < INTEGERRS_CAP; ++i) {
    auto rs = curCPUstate.IntegerRS[i];
    if (!rs.free && rs.qj == -1 && rs.qk == -1) {
      if (rs.ROB_dest < Execute_IntegerRS.ROB_dest) {
        Execute_IntegerRS = rs;
        Execute_IntegerRS_index = i;
      }
    }
  }
  if (Execute_IntegerRS.op == OP_INVALID) {
    return;
  }
  nextCPUstate.ALUModule.ALUExecute(Execute_IntegerRS.vj, Execute_IntegerRS.vk,
                                    Execute_IntegerRS.op,
                                    Execute_IntegerRS.ROB_dest);
  nextCPUstate.IntegerRS[Execute_IntegerRS_index].free = true;
}

void CPU::writeBack() {
  if (curCPUstate.ALUModule.isEmpty())
    return;
  auto result = curCPUstate.ALUModule.pop();
  auto tag = result.robTag;
  auto value = result.value;
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
  nextCPUstate.ROBModule.writeROB(value, curCPUstate.ROBModule.getIndex(tag),
                                  Ready);
}

void CPU::commit() {
  if (nextCPUstate.ROBModule.peek().state != Ready)
    return;
  auto rob_entry = nextCPUstate.ROBModule.pop();
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
    nextCPUstate.reg[0].write(0);
    if (finish) {
      std::cout << std::dec << (curCPUstate.reg[result].read() & 0xFF)
                << std::endl;
      std::cout << "Clock cycles: " << clock << std::endl;
    }
    curCPUstate = nextCPUstate;
    ++clock;
  }
}