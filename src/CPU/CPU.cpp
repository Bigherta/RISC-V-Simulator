#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cstdint>
#include <iostream>

CPU::CPU(Memory mem) : CPUstate(mem), InstructMem(mem), DataMem(mem) {}

void CPU::fetch() {
  if (squashDetect.needSquash) {
    CPUstate.INQModule.clear();
    CPUstate.programCounter = squashDetect.SquashPC;
    CPUstate.haltFetched = false;
    return;
  }
  if (haltFetched)
    return;
  const auto first_byte =
      static_cast<uint32_t>(InstructMem.read_data(programCounter));
  const auto second_byte =
      static_cast<uint32_t>(InstructMem.read_data(programCounter + 1));
  const auto third_byte =
      static_cast<uint32_t>(InstructMem.read_data(programCounter + 2));
  const auto fourth_byte =
      static_cast<uint32_t>(InstructMem.read_data(programCounter + 3));
  const auto raw_inst = first_byte | (second_byte << 8) | (third_byte << 16) |
                        (fourth_byte << 24);
  if (raw_inst == 0x0ff00513)
    CPUstate.haltFetched = true;
  if (!INQModule.isFull()) {
    CPUstate.INQModule.push(raw_inst, programCounter);
    CPUstate.programCounter = programCounter + 4;
  }
}

void CPU::decode() {
  if (squashDetect.needSquash) {
    return;
  }
  auto detected = INQModule.decodeDetect();
  if (detected >= 0) {
    CPUstate.INQModule.decode(detected);
  }
}

IssueResult CPU::issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk,
                                 bool isControl) {
  if (RSModule.isIntergerRSFull() || ROBModule.isFull()) {
    return {false, inst.rd, -1};
  }
  ReservationStation IntegerRS{};
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto destination = inst.rd;
  auto op1 = REGModule.readOperand(regNum1);
  if (op1.ready) {
    IntegerRS.vj = op1.value;
  } else {
    auto op1Index = ROBModule.getIndex(op1.robTag);
    if (op1Index != -1 &&
        (ROBModule.getEntry(op1Index).state >= ROBState::ValueReady)) {
      IntegerRS.vj = ROBModule.getEntry(op1Index).value;
    } else {
      auto bypass = CDBBypass(op1.robTag);
      if (bypass.valid) {
        IntegerRS.vj = bypass.value;
      } else {
        IntegerRS.qj = op1.robTag;
      }
    }
  }
  if (imm_as_vk) {
    IntegerRS.vk = inst.imm;
  } else if (has_rs2) {
    auto op2 = REGModule.readOperand(regNum2);
    if (op2.ready) {
      IntegerRS.vk = op2.value;
    } else {
      auto op2Index = ROBModule.getIndex(op2.robTag);
      if (op2Index != -1 &&
          (ROBModule.getEntry(op2Index).state >= ROBState::ValueReady)) {
        IntegerRS.vk = ROBModule.getEntry(op2Index).value;
      } else {
        auto bypass = CDBBypass(op2.robTag);
        if (bypass.valid) {
          IntegerRS.vk = bypass.value;
        } else {
          IntegerRS.qk = op2.robTag;
        }
      }
    }
  }

  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.predictedPC = inst.pc + 4;
  if (isControl) {
    newROB.value = inst.pc + 4;
    newROB.type = ROBType::LINK;
    newROB.state = ROBState::ValueReady;
  }
  newROB.tag = CPUstate.ROBModule.push(newROB);
  IntegerRS.ROB_dest = newROB.tag;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (RSModule.IntegerRS[i].free) {
      CPUstate.RSModule.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return {destination != 0, destination, newROB.tag};
}

IssueResult CPU::issue_UandJ(Instruct inst, bool has_PC, bool isControl) {
  if (RSModule.isIntergerRSFull() || ROBModule.isFull()) {
    return {false, inst.rd, -1};
  }
  ReservationStation IntegerRS{};
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  if (has_PC) {
    IntegerRS.vj = inst.pc;
  }
  IntegerRS.vk = inst.imm;
  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.predictedPC = inst.pc + 4;
  if (isControl) {
    newROB.value = inst.pc + 4;
    newROB.type = ROBType::LINK;
    newROB.state = ROBState::ValueReady;
  }
  newROB.tag = CPUstate.ROBModule.push(newROB);
  IntegerRS.ROB_dest = newROB.tag;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (RSModule.IntegerRS[i].free) {
      CPUstate.RSModule.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return {destination != 0, destination, newROB.tag};
}

IssueResult CPU::issue_B(Instruct inst) {
  if (RSModule.isBranchRSFull() || ROBModule.isFull()) {
    return {false, inst.rd, -1};
  }
  BranchReservationStation BranchRS{};
  BranchRS.free = false;
  BranchRS.op = decodeOp(inst);
  BranchRS.imm = inst.imm;
  BranchRS.pc = inst.pc;
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto op1 = REGModule.readOperand(regNum1);
  if (op1.ready) {
    BranchRS.vj = op1.value;
  } else {
    auto op1Index = ROBModule.getIndex(op1.robTag);
    if (op1Index != -1 &&
        (ROBModule.getEntry(op1Index).state >= ROBState::ValueReady)) {
      BranchRS.vj = ROBModule.getEntry(op1Index).value;
    } else {
      auto bypass = CDBBypass(op1.robTag);
      if (bypass.valid) {
        BranchRS.vj = bypass.value;
      } else {
        BranchRS.qj = op1.robTag;
      }
    }
  }
  auto op2 = REGModule.readOperand(regNum2);
  if (op2.ready) {
    BranchRS.vk = op2.value;
  } else {
    auto op2Index = ROBModule.getIndex(op2.robTag);
    if (op2Index != -1 &&
        (ROBModule.getEntry(op2Index).state >= ROBState::ValueReady)) {
      BranchRS.vk = ROBModule.getEntry(op2Index).value;
    } else {
      auto bypass = CDBBypass(op2.robTag);
      if (bypass.valid) {
        BranchRS.vk = bypass.value;
      } else {
        BranchRS.qk = op2.robTag;
      }
    }
  }
  ROBEntry newROB(ROBType::BRANCH);
  newROB.predictedPC = inst.pc + 4;
  newROB.tag = CPUstate.ROBModule.push(newROB);
  BranchRS.ROB_dest = newROB.tag;
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (RSModule.BranchRS[i].free) {
      CPUstate.RSModule.BranchRS[i] = BranchRS;
      break;
    }
  }
  return {false, 0, newROB.tag};
}

IssueResult CPU::issue_Load(Instruct inst, int n_bytes, bool isUnsigned) {
  if (RSModule.isLoadRSFull() || ROBModule.isFull() || LSQModule.isFull()) {
    return {false, inst.rd, -1};
  }
  ReservationStation LoadRS{};
  LoadRS.free = false;
  LoadRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto destination = inst.rd;

  auto op1 = REGModule.readOperand(regNum1);
  if (op1.ready) {
    LoadRS.vj = op1.value;
  } else {
    auto op1Index = ROBModule.getIndex(op1.robTag);
    if (op1Index != -1 &&
        (ROBModule.getEntry(op1Index).state >= ROBState::ValueReady)) {
      LoadRS.vj = ROBModule.getEntry(op1Index).value;
    } else {
      auto bypass = CDBBypass(op1.robTag);
      if (bypass.valid) {
        LoadRS.vj = bypass.value;
      } else {
        LoadRS.qj = op1.robTag;
      }
    }
  }
  LoadRS.vk = inst.imm;

  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.tag = CPUstate.ROBModule.push(newROB);
  LoadRS.ROB_dest = newROB.tag;
  LSQEntry newLSQ{};
  newLSQ.isAddressReady = false;
  newLSQ.valueState = ValueState::NOTREADY;
  newLSQ.ROBTag = newROB.tag;
  newLSQ.type = Operation::Load;
  newLSQ.n_bytes = n_bytes;
  newLSQ.isUnsigned = isUnsigned;
  CPUstate.LSQModule.push(newLSQ);
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (RSModule.LoadRS[i].free) {
      CPUstate.RSModule.LoadRS[i] = LoadRS;
      break;
    }
  }
  return {destination != 0, destination, newROB.tag};
}

IssueResult CPU::issue_Store(Instruct inst, int n_bytes) {

  if (RSModule.isStoreRSFull() || RSModule.isMicroStoreRSFull() ||
      ROBModule.isFull() || LSQModule.isFull()) {
    return {false, 0, -1};
  }
  ReservationStation StoreRS{};
  StoreRS.free = false;
  StoreRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;

  auto op1 = REGModule.readOperand(regNum1);
  if (op1.ready) {
    StoreRS.vj = op1.value;
  } else {
    auto op1Index = ROBModule.getIndex(op1.robTag);
    if (op1Index != -1 &&
        (ROBModule.getEntry(op1Index).state >= ROBState::ValueReady)) {
      StoreRS.vj = ROBModule.getEntry(op1Index).value;
    } else {
      auto bypass = CDBBypass(op1.robTag);
      if (bypass.valid) {
        StoreRS.vj = bypass.value;
      } else {
        StoreRS.qj = op1.robTag;
      }
    }
  }
  StoreRS.vk = inst.imm;

  StoreMicroReservationStation MicroRS{};
  MicroRS.free = false;
  auto op2 = REGModule.readOperand(regNum2);
  if (op2.ready) {
    MicroRS.vrs2 = op2.value;
  } else {
    auto op2Index = ROBModule.getIndex(op2.robTag);
    if (op2Index != -1 &&
        (ROBModule.getEntry(op2Index).state >= ROBState::ValueReady)) {
      MicroRS.vrs2 = ROBModule.getEntry(op2Index).value;
    } else {
      auto bypass = CDBBypass(op2.robTag);
      if (bypass.valid) {
        MicroRS.vrs2 = bypass.value;
      } else {
        MicroRS.qrs2 = op2.robTag;
      }
    }
  }

  ROBEntry newROB(ROBType::STORE);
  newROB.tag = CPUstate.ROBModule.push(newROB);
  StoreRS.ROB_dest = newROB.tag;
  MicroRS.ROB_dest = newROB.tag;
  LSQEntry newLSQ{};
  newLSQ.type = Operation::Store;
  newLSQ.ROBTag = newROB.tag;
  newLSQ.n_bytes = n_bytes;
  newLSQ.isAddressReady = false;
  newLSQ.valueState = ValueState::NOTREADY;
  CPUstate.LSQModule.push(newLSQ);
  for (int i = 0; i < STORERS_CAP; i++) {
    if (RSModule.StoreRS[i].free) {
      CPUstate.RSModule.StoreRS[i] = StoreRS;
      break;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (RSModule.MicroStoreRS[i].free) {
      CPUstate.RSModule.MicroStoreRS[i] = MicroRS;
      break;
    }
  }
  return {false, 0, newROB.tag};
}

void CPU::issue() {
  if (!squashDetect.needSquash) {
    RATWritePort commitPort{};
    if (!ROBModule.isEmpty() &&
        ROBModule.headState() == ROBState::CommitReady) {
      auto rob_entry = ROBModule.peek();
      if ((rob_entry.type == ROBType::REGISTER ||
           rob_entry.type == ROBType::LINK) &&
          REGModule.readRAT(rob_entry.dest) == rob_entry.tag) {
        commitPort = {true, (uint32_t)rob_entry.dest, -1};
      }
    }

    IssueResult res{false, 0, -1};
    if (!INQModule.isEmpty() && INQModule.headDecoded()) {
      Instruct inst = INQModule.peek();
      switch (inst.type) {
      case RISC_V::R: {
        res = issue_IntegerRS(inst, true, false, false);
        break;
      }
      case RISC_V::I: {
        if (inst.opcode == 0b0010011) {
          if (inst.isHalt) {
            ROBEntry newROB(ROBType::REGISTER);
            newROB.dest = inst.rd;
            newROB.halt = true;
            newROB.state = ROBState::CommitReady;
            newROB.tag = CPUstate.ROBModule.push(newROB);
            res = {false, inst.rd, newROB.tag};
          } else {
            res = issue_IntegerRS(inst, false, true, false);
          }
        } else if (inst.opcode == 0b1100111) {
          res = issue_IntegerRS(inst, false, true, true);
        } else if (inst.opcode == 0b0000011) {
          int n_bytes = 4;
          bool isUnsigned = false;
          switch (inst.funct3) {
          case 0b000:
            n_bytes = 1;
            isUnsigned = false;
            break;
          case 0b001:
            n_bytes = 2;
            isUnsigned = false;
            break;
          case 0b010:
            n_bytes = 4;
            isUnsigned = false;
            break;
          case 0b100:
            n_bytes = 1;
            isUnsigned = true;
            break;
          case 0b101:
            n_bytes = 2;
            isUnsigned = true;
            break;
          default:
            break;
          }
          res = issue_Load(inst, n_bytes, isUnsigned);
        }
        break;
      }
      case RISC_V::Istar: {
        res = issue_IntegerRS(inst, false, true, false);
        break;
      }
      case RISC_V::S: {
        int n_bytes = 4;
        switch (inst.funct3) {
        case 0b000:
          n_bytes = 1;
          break;
        case 0b001:
          n_bytes = 2;
          break;
        case 0b010:
          n_bytes = 4;
          break;
        default:
          break;
        }
        res = issue_Store(inst, n_bytes);
        break;
      }
      case RISC_V::B: {
        res = issue_B(inst);
        break;
      }
      case RISC_V::U: {
        if (inst.opcode == 0b0010111) {
          res = issue_UandJ(inst, true, false);
        } else if (inst.opcode == 0b0110111) {
          res = issue_UandJ(inst, false, false);
        }
        break;
      }
      case RISC_V::J: {
        res = issue_UandJ(inst, true, true);
        break;
      }
      case RISC_V::RV_INVALID: {
        CPUstate.INQModule.pop();
        res = {false, 0, -1};
        break;
      }
      }
    }
    if (res.tag != -1)
      CPUstate.INQModule.pop();

    RATWritePort issuePort{res.valid, (uint32_t)res.rd, res.tag};

    auto ports = RATSEL::RATWrite(issuePort, commitPort);
    if (ports.first.valid)
      CPUstate.REGModule.setRAT(ports.first.reg, ports.first.value);
    if (ports.second.valid)
      CPUstate.REGModule.setRAT(ports.second.reg, ports.second.value);
  }
}

Operation CPU::decodeOp(Instruct inst) {
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

void CPU::execute() {
  // ALU execute
  if (!ALUModule.isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    ReservationStation Execute_RS{};
    Execute_RS.ROB_dest = ~0u >> 1;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = RSModule.IntegerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
        }
      }
    }
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = RSModule.LoadRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = RSModule.StoreRS[i];
      if (!rs.free && rs.qj == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash &&
           Execute_RS.ROB_dest < squashDetect.SquashTag)) {
        auto result =
            ALU::ALUCalculate(Execute_RS.vj, Execute_RS.vk, Execute_RS.op);
        auto index = ROBModule.getIndex(Execute_RS.ROB_dest);
        if (Execute_RS_type == 0 && !isMemoryOp(Execute_RS.op) &&
            !isControlOp(Execute_RS.op) && index >= 0 &&
            ROBModule.getEntry(index).type == ROBType::REGISTER) {
          CPUstate.ROBModule.writeROBState(ROBState::ValueReady, index);
          CPUstate.ROBModule.writeROBValue(result, index);
        }
        CPUstate.ALUModule.push({result, Execute_RS.ROB_dest,
                                 isMemoryOp(Execute_RS.op),
                                 isControlOp(Execute_RS.op)});
        switch (Execute_RS_type) {
        case 0:
          CPUstate.RSModule.IntegerRS[Execute_RS_index].free = true;
          break;
        case 1:
          CPUstate.RSModule.LoadRS[Execute_RS_index].free = true;
          break;
        case 2:
          CPUstate.RSModule.StoreRS[Execute_RS_index].free = true;
          break;
        }
      }
    }
  }
  // store execute
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (!RSModule.MicroStoreRS[i].free && RSModule.MicroStoreRS[i].qrs2 == -1 &&
        (!squashDetect.needSquash ||
         (squashDetect.needSquash &&
          RSModule.MicroStoreRS[i].ROB_dest < squashDetect.SquashTag))) {
      auto index = LSQModule.getIndex(RSModule.MicroStoreRS[i].ROB_dest);
      CPUstate.LSQModule.writeValue(RSModule.MicroStoreRS[i].vrs2, index);
      CPUstate.LSQModule.DataBroadcast(index);
      CPUstate.RSModule.MicroStoreRS[i].free = true;
    }
  }
  // BRU execute
  if (!BRUModule.isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    BranchReservationStation Execute_RS{};
    Execute_RS.ROB_dest = ~0u >> 1;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = RSModule.BranchRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash &&
           Execute_RS.ROB_dest < squashDetect.SquashTag)) {
        CPUstate.BRUModule.BRUExecute(Execute_RS.vj, Execute_RS.vk,
                                      Execute_RS.pc, Execute_RS.imm,
                                      Execute_RS.op, Execute_RS.ROB_dest);
        CPUstate.RSModule.BranchRS[Execute_RS_index].free = true;
      }
    }
  }
  // MEM execute
  CPUstate.DataMem.execute();
  //  store and load execute
  bool memBusy = DataMem.isBusy();
  if (!memBusy && !LSQModule.isEmpty() &&
      LSQModule.peek().type == Operation::Store) {
    auto storeEntry = LSQModule.peek();
    int storeIndex = ROBModule.getIndex(storeEntry.ROBTag);
    if (storeIndex == -1 ||
        (storeIndex == ROBModule.getHead() &&
         ROBModule.getEntry(storeIndex).state == ROBState::CommitReady)) {
      MemRequest newRequest{};
      newRequest.address = storeEntry.address;
      newRequest.value = storeEntry.value;
      newRequest.isSigned = !storeEntry.isUnsigned;
      newRequest.n_bytes = storeEntry.n_bytes;
      newRequest.op = Operation::Store;
      newRequest.ROBTag = storeEntry.ROBTag;
      if (CPUstate.DataMem.MemPush(newRequest)) {
        if (debug::enabled(debug::TOPIC_MEM))
          debug::print("MEM store @%u <- %d\n", newRequest.address,
                       newRequest.value);
        CPUstate.LSQModule.pop();
        memBusy = true;
      }
    }
  }
  auto loadIndex = LSQModule.LoadDetect();
  if (loadIndex != 0xFFFFFFFF && !memBusy) {
    MemRequest newRequest{};
    auto entry = LSQModule.getEntry(loadIndex);
    newRequest.address = entry.address;
    newRequest.isSigned = !entry.isUnsigned;
    newRequest.n_bytes = entry.n_bytes;
    newRequest.op = Operation::Load;
    newRequest.ROBTag = entry.ROBTag;
    if (!squashDetect.needSquash ||
        (squashDetect.needSquash &&
         newRequest.ROBTag < squashDetect.SquashTag)) {
      if (CPUstate.DataMem.MemPush(newRequest))
        CPUstate.LSQModule.setValueState(loadIndex, ValueState::FETCHING);
    }
  }
  // LSQ execute
  for (int i = LSQModule.getHead(); i != LSQModule.getTail();
       i = (i + 1) & 0x3F) {
    auto e = LSQModule.getEntry(i);
    if (LSQModule.isReadyToCommit(i)) {
      auto lsqEntry = LSQModule.getEntry(i);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash &&
           lsqEntry.ROBTag < squashDetect.SquashTag)) {
        auto index = ROBModule.getIndex(lsqEntry.ROBTag);
        if (index != -1) {
          CPUstate.ROBModule.writeROBValue(lsqEntry.value, index);
          CPUstate.ROBModule.writeROBState(ROBState::CommitReady, index);
        }
      }
    }
  }
}

void CPU::CDBBroadcast(int tag, int value) {
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (RSModule.IntegerRS[i].qj == tag) {
      CPUstate.RSModule.IntegerRS[i].vj = value;
      CPUstate.RSModule.IntegerRS[i].qj = -1;
    }
    if (RSModule.IntegerRS[i].qk == tag) {
      CPUstate.RSModule.IntegerRS[i].vk = value;
      CPUstate.RSModule.IntegerRS[i].qk = -1;
    }
  }
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (RSModule.LoadRS[i].qj == tag) {
      CPUstate.RSModule.LoadRS[i].vj = value;
      CPUstate.RSModule.LoadRS[i].qj = -1;
    }
    if (RSModule.LoadRS[i].qk == tag) {
      CPUstate.RSModule.LoadRS[i].vk = value;
      CPUstate.RSModule.LoadRS[i].qk = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (RSModule.StoreRS[i].qj == tag) {
      CPUstate.RSModule.StoreRS[i].vj = value;
      CPUstate.RSModule.StoreRS[i].qj = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (RSModule.MicroStoreRS[i].qrs2 == tag) {
      CPUstate.RSModule.MicroStoreRS[i].vrs2 = value;
      CPUstate.RSModule.MicroStoreRS[i].qrs2 = -1;
    }
  }
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (RSModule.BranchRS[i].qj == tag) {
      CPUstate.RSModule.BranchRS[i].vj = value;
      CPUstate.RSModule.BranchRS[i].qj = -1;
    }
    if (RSModule.BranchRS[i].qk == tag) {
      CPUstate.RSModule.BranchRS[i].vk = value;
      CPUstate.RSModule.BranchRS[i].qk = -1;
    }
  }
}

CDBBypassResult CPU::CDBBypass(int robTag) const {
  CDBBypassResult out;
  if (cdbArbiter.valid && !cdbArbiter.result.isAddress &&
      !cdbArbiter.result.isControl &&
      cdbArbiter.result.robTag == robTag) {
    out.valid = true;
    out.value = cdbArbiter.result.value;
  }
  return out;
}

void CPU::writeBack() {
  if (DataMem.isReady()) {
    auto reply = DataMem.MemReturn();
    if (reply.op == Operation::Load &&
        (!squashDetect.needSquash ||
         (squashDetect.needSquash && reply.ROBTag < squashDetect.SquashTag))) {
      auto index = LSQModule.getIndex(reply.ROBTag);
      if (index >= 0) {
        if (debug::enabled(debug::TOPIC_MEM))
          debug::print("MEM load @%u <- %d\n", reply.address, reply.value);
        CPUstate.LSQModule.writeValue(reply.value, index);
      }
    }
    CPUstate.DataMem.MemPull();
  }
  SquashInfo BranchSquash;
  if (!BRUModule.isEmpty()) {
    auto BranchResult = BRUModule.peek();
    auto index = ROBModule.getIndex(BranchResult.robTag);
    if (index >= 0 && (!squashDetect.needSquash ||
                       (squashDetect.needSquash &&
                        BranchResult.robTag < squashDetect.SquashTag))) {
      auto actualPC = BranchResult.pcResult;
      if (actualPC != ROBModule.getPredictedPC(index)) {
        BranchSquash.needSquash = true;
        BranchSquash.SquashPC = actualPC;
        BranchSquash.SquashTag = BranchResult.robTag;
      }
      CPUstate.ROBModule.writeROBState(ROBState::CommitReady, index);
    }
    CPUstate.BRUModule.pop();
  }

  CDBOutput cdbOut = cdbArbiter;
  if (!cdbOut.valid) {
    if (BranchSquash.needSquash) {
      CPUstate.flushArbiter.receive(BranchSquash);
    }
    return;
  }

  if (cdbOut.aluGranted)
    CPUstate.ALUModule.pop();
  if (squashDetect.needSquash &&
      cdbOut.result.robTag > squashDetect.SquashTag) {
    if (BranchSquash.needSquash) {
      CPUstate.flushArbiter.receive(BranchSquash);
    }
    return;
  }
  auto tag = cdbOut.result.robTag;
  auto isAddress = cdbOut.result.isAddress;
  auto isControl = cdbOut.result.isControl;
  SquashInfo JumpSquash;
  if (!isAddress && !isControl) {
    auto value = cdbOut.result.value;
    auto index = ROBModule.getIndex(tag);
    CDBBroadcast(tag, value);
    if (index != -1) {
      CPUstate.ROBModule.writeROBState(ROBState::CommitReady, index);
    }
    if (cdbOut.lsqGranted) {
      auto lsqIndex = LSQModule.getIndex(tag);
      if (lsqIndex >= 0)
        CPUstate.LSQModule.setCDBBroadcast(lsqIndex);
    }
  } else if (isAddress) {
    auto value = cdbOut.result.value;
    auto index = LSQModule.getIndex(tag);
    if (index >= 0) {
      CPUstate.LSQModule.writeAddress(value, index);
      CPUstate.LSQModule.AddressBroadcast(index);
    }
  } else if (isControl) {
    const auto robTag = tag;
    const auto robIndex = ROBModule.getIndex(robTag);
    if (robIndex >= 0) {
      const auto robEntry = ROBModule.getEntry(robIndex);
      const auto pc = static_cast<uint32_t>(cdbOut.result.value);
      const auto value = robEntry.value;

      CDBBroadcast(robTag, value);
      if (pc != ROBModule.getPredictedPC(robIndex)) {
        JumpSquash.needSquash = true;
        JumpSquash.SquashPC = pc;
        JumpSquash.SquashTag = robTag;
      }
      CPUstate.ROBModule.writeROBState(ROBState::CommitReady, robIndex);
    }
  }
  if (BranchSquash.needSquash && JumpSquash.needSquash) {
    CPUstate.flushArbiter.receive(BranchSquash.SquashTag < JumpSquash.SquashTag
                                      ? BranchSquash
                                      : JumpSquash);
  } else {
    if (BranchSquash.needSquash)
      CPUstate.flushArbiter.receive(BranchSquash);
    if (JumpSquash.needSquash)
      CPUstate.flushArbiter.receive(JumpSquash);
  }
}

void CPU::commit() {
  if (squashDetect.needSquash) {
    CPUstate.ROBModule.flush(squashDetect.SquashTag);
    return;
  }
  uint8_t cur = LSQModule.getHead();
  while (cur != LSQModule.getTail()) {
    LSQEntry e = LSQModule.getEntry(cur);
    if (e.type == Operation::Load && ROBModule.getIndex(e.ROBTag) == -1 &&
        e.isCDBBroadcast) {
      CPUstate.LSQModule.pop();
      cur = (cur + 1) & 0x3F;
    } else {
      break;
    }
  }
  if (ROBModule.isEmpty() || ROBModule.headState() != ROBState::CommitReady)
    return;
  auto rob_entry = ROBModule.peek();
  rob_entry = CPUstate.ROBModule.pop();
  if (rob_entry.halt) {
    CPUstate.haltCommitted = true;
    CPUstate.haltRd = rob_entry.dest;
  } else if (rob_entry.type == ROBType::REGISTER ||
             rob_entry.type == ROBType::LINK) {
    CPUstate.REGModule.writeReg(rob_entry.dest, rob_entry.value);
  }
}
void CPU::read() {
  RSModule = CPUstate.RSModule;
  REGModule = CPUstate.REGModule;
  ROBModule = CPUstate.ROBModule;
  ALUModule = CPUstate.ALUModule;
  BRUModule = CPUstate.BRUModule;
  LSQModule = CPUstate.LSQModule;
  INQModule = CPUstate.INQModule;
  DataMem.snapshotFrom(CPUstate.DataMem);
  programCounter = CPUstate.programCounter;
  haltFetched = CPUstate.haltFetched;
  haltCommitted = CPUstate.haltCommitted;
  haltRd = CPUstate.haltRd;
  flushArbiter = CPUstate.flushArbiter;
  squashDetect = CPUstate.flushArbiter.arbitResult();
  cdbArbiter = CDBArbiter::arbitrate(ALUModule, LSQModule, squashDetect);
}
void CPU::flush() {
  if (squashDetect.needSquash) {
    // 1. clear the wrong RS
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!RSModule.IntegerRS[i].free &&
          RSModule.IntegerRS[i].ROB_dest > squashDetect.SquashTag) {
        CPUstate.RSModule.IntegerRS[i].free = true;
      }
    }
    for (int i = 0; i < LOADRS_CAP; i++) {
      if (!RSModule.LoadRS[i].free &&
          RSModule.LoadRS[i].ROB_dest > squashDetect.SquashTag) {
        CPUstate.RSModule.LoadRS[i].free = true;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!RSModule.StoreRS[i].free &&
          RSModule.StoreRS[i].ROB_dest > squashDetect.SquashTag) {
        CPUstate.RSModule.StoreRS[i].free = true;
      }
    }
    for (int i = 0; i < BRANCHRS_CAP; i++) {
      if (!RSModule.BranchRS[i].free &&
          RSModule.BranchRS[i].ROB_dest > squashDetect.SquashTag) {
        CPUstate.RSModule.BranchRS[i].free = true;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!RSModule.MicroStoreRS[i].free &&
          RSModule.MicroStoreRS[i].ROB_dest > squashDetect.SquashTag) {
        CPUstate.RSModule.MicroStoreRS[i].free = true;
      }
    }
    // 2. clear the wrong LSQ
    CPUstate.LSQModule.flush(squashDetect.SquashTag);
    // 3. clear the wrong RAT
    for (int regNum = 0; regNum < REGISTER_CAP; ++regNum) {
      if (REGModule.readRAT(regNum) > squashDetect.SquashTag) {
        const auto head = ROBModule.getHead();
        const auto tail = ROBModule.getTail();
        bool repaired = false;
        for (int index = head; index != tail;
             index = (index + 1) & (ROB_CAP - 1)) {
          auto entry = ROBModule.getEntry(index);
          if ((entry.type == ROBType::REGISTER ||
               entry.type == ROBType::LINK) &&
              entry.tag <= squashDetect.SquashTag && entry.dest == regNum) {
            CPUstate.REGModule.setRAT(regNum, entry.tag);
            repaired = true;
          }
        }
        if (!repaired) {
          CPUstate.REGModule.setRAT(regNum, -1);
        }
      }
    }
    // 4. clear the wrong ALU outputBuffer
    CPUstate.ALUModule.flush(squashDetect.SquashTag);
    // 5. clear the wrong BRU outputBuffer
    CPUstate.BRUModule.flush(squashDetect.SquashTag);
    // 6. clear the old flushArbiter elements
    CPUstate.flushArbiter.clear(squashDetect.SquashTag);
  }
}
void CPU::run() {
  bool finish = false;
  uint64_t clock = 0;
  while (!finish) {
    read();
    issue();
    commit();
    writeBack();
    flush();
    execute();
    decode();
    fetch();
    CPUstate.REGModule.resetX0();
    ++clock;
    finish = haltCommitted && INQModule.isEmpty() && ROBModule.isEmpty();
  }
  std::cout << std::dec << (REGModule.readReg(haltRd) & 0xFF) << std::endl;
}