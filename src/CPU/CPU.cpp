#include "../include/CPU.hpp"
#include "../include/Decoder.hpp"
#include "../include/util.hpp"
#include <cstdint>
#include <iostream>

CPU::CPU(Memory mem) : curCPUstate(mem), nextCPUstate(mem) {}

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
  ReservationStation IntegerRS{};
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto destination = inst.rd;

  auto op1 = curCPUstate.REGModule.readOperand(regNum1);
  if (op1.ready) {
    IntegerRS.vj = op1.value;
  } else {
    auto robEntry = curCPUstate.ROBModule.getEntry(op1.robTag);
    if (robEntry.state == ROBState::Ready) {
      IntegerRS.vj = robEntry.value;
    } else {
      IntegerRS.qj = op1.robTag;
    }
  }
  DPRINT(TOPIC_ISSUE, "ISSUE int rs1=%d ready=%d v=%d q=%d\n", regNum1,
         op1.ready, op1.value, op1.robTag);

  if (imm_as_vk) {
    IntegerRS.vk = inst.imm;
  } else if (has_rs2) {
    auto op2 = curCPUstate.REGModule.readOperand(regNum2);
    if (op2.ready) {
      IntegerRS.vk = op2.value;
    } else {
      auto robEntry = curCPUstate.ROBModule.getEntry(op2.robTag);
      if (robEntry.state == ROBState::Ready) {
        IntegerRS.vk = robEntry.value;
      } else {
        IntegerRS.qk = op2.robTag;
      }
    }
  }

  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.tag = nextCPUstate.ROBModule.push(newROB);
  nextCPUstate.REGModule.setRAT(destination, newROB.tag);
  IntegerRS.ROB_dest = newROB.tag;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (curCPUstate.IntegerRS[i].free) {
      nextCPUstate.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return true;
}

bool CPU::issue_IntegerU(Instruct inst, bool has_PC) {
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
  ReservationStation IntegerRS{};
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  if (has_PC) {
    IntegerRS.vj = curCPUstate.programCounter;
  }
  IntegerRS.vk = inst.imm;
  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.tag = nextCPUstate.ROBModule.push(newROB);
  nextCPUstate.REGModule.setRAT(destination, newROB.tag);
  IntegerRS.ROB_dest = newROB.tag;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (curCPUstate.IntegerRS[i].free) {
      nextCPUstate.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return true;
}

bool CPU::issue_Load(Instruct inst, int n_bytes, bool isUnsigned) {
  bool isFull = true;
  for (auto LoadRS : curCPUstate.LoadRS) {
    if (LoadRS.free) {
      isFull = false;
      break;
    }
  }
  if (isFull || curCPUstate.ROBModule.isFull() ||
      curCPUstate.LSQModule.isFull()) {
    return false;
  }
  ReservationStation LoadRS{};
  LoadRS.free = false;
  LoadRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto destination = inst.rd;

  auto op1 = curCPUstate.REGModule.readOperand(regNum1);
  if (op1.ready) {
    LoadRS.vj = op1.value;
  } else {
    auto robEntry = curCPUstate.ROBModule.getEntry(op1.robTag);
    if (robEntry.state == ROBState::Ready) {
      LoadRS.vj = robEntry.value;
    } else {
      LoadRS.qj = op1.robTag;
    }
  }
  LoadRS.vk = inst.imm;

  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.tag = nextCPUstate.ROBModule.push(newROB);
  nextCPUstate.REGModule.setRAT(destination, newROB.tag);
  LoadRS.ROB_dest = newROB.tag;
  LSQEntry newLSQ;
  newLSQ.isAddressReady = false;
  newLSQ.valueState = ValueState::NOTREADY;
  newLSQ.ROBTag = newROB.tag;
  newLSQ.type = Operation::Load;
  newLSQ.n_bytes = n_bytes;
  newLSQ.isUnsigned = isUnsigned;
  nextCPUstate.LSQModule.push(newLSQ);
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (curCPUstate.LoadRS[i].free) {
      nextCPUstate.LoadRS[i] = LoadRS;
      break;
    }
  }
  return true;
}

bool CPU::issue_Store(Instruct inst, int n_bytes) {
  DPRINT(TOPIC_ISSUE, "ISSUE store rs2=%d imm=%d\n", inst.rs2, inst.imm);
  bool isFull = true;
  for (auto StoreRS_elem : curCPUstate.StoreRS) {
    if (StoreRS_elem.free) {
      isFull = false;
      break;
    }
  }
  bool microFull = true;
  for (auto MicroRS : curCPUstate.MicroStoreRS) {
    if (MicroRS.free) {
      microFull = false;
      break;
    }
  }
  if (isFull || microFull || curCPUstate.ROBModule.isFull() ||
      curCPUstate.LSQModule.isFull()) {
    return false;
  }
  ReservationStation StoreRS{};
  StoreRS.free = false;
  StoreRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;

  auto op1 = curCPUstate.REGModule.readOperand(regNum1);
  if (op1.ready) {
    StoreRS.vj = op1.value;
  } else {
    auto robEntry = curCPUstate.ROBModule.getEntry(op1.robTag);
    if (robEntry.state == ROBState::Ready) {
      StoreRS.vj = robEntry.value;
    } else {
      StoreRS.qj = op1.robTag;
    }
  }
  StoreRS.vk = inst.imm;

  StoreMicroReservationStation MicroRS{};
  MicroRS.free = false;
  auto op2 = curCPUstate.REGModule.readOperand(regNum2);
  if (op2.ready) {
    MicroRS.vrs2 = op2.value;
    DPRINT(TOPIC_ISSUE, "ISSUE store rs2 ready v=%d\n", op2.value);
  } else {
    auto robEntry = curCPUstate.ROBModule.getEntry(op2.robTag);
    if (robEntry.state == ROBState::Ready) {
      MicroRS.vrs2 = robEntry.value;
      DPRINT(TOPIC_ISSUE, "ISSUE store rs2 rob ready v=%d\n", robEntry.value);
    } else {
      MicroRS.qrs2 = op2.robTag;
      DPRINT(TOPIC_ISSUE, "ISSUE store rs2 tag q=%d\n", op2.robTag);
    }
  }

  ROBEntry newROB(ROBType::STORE);
  newROB.tag = nextCPUstate.ROBModule.push(newROB);
  StoreRS.ROB_dest = newROB.tag;
  MicroRS.ROB_dest = newROB.tag;
  LSQEntry newLSQ;
  newLSQ.type = Operation::Store;
  newLSQ.ROBTag = newROB.tag;
  newLSQ.n_bytes = n_bytes;
  newLSQ.isAddressReady = false;
  newLSQ.valueState = ValueState::NOTREADY;
  nextCPUstate.LSQModule.push(newLSQ);
  for (int i = 0; i < STORERS_CAP; i++) {
    if (curCPUstate.StoreRS[i].free) {
      nextCPUstate.StoreRS[i] = StoreRS;
      break;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (curCPUstate.MicroStoreRS[i].free) {
      nextCPUstate.MicroStoreRS[i] = MicroRS;
      break;
    }
  }
  return true;
}

int CPU::issue() {
  DPRINT(TOPIC_ISSUE, "ISSUE pc=%d\n", curCPUstate.programCounter);
  auto pc = curCPUstate.programCounter;
  auto first_byte =
      static_cast<uint32_t>(curCPUstate.InstructMem.read_data(pc));
  auto second_byte =
      static_cast<uint32_t>(curCPUstate.InstructMem.read_data(pc + 1));
  auto third_byte =
      static_cast<uint32_t>(curCPUstate.InstructMem.read_data(pc + 2));
  auto fourth_byte =
      static_cast<uint32_t>(curCPUstate.InstructMem.read_data(pc + 3));
  auto raw_inst = first_byte | (second_byte << 8) | (third_byte << 16) |
                  (fourth_byte << 24);
  auto inst = Decoder::decode(raw_inst);
  nextCPUstate.PCWriteEnable = false;
  if (raw_inst == 0x0ff00513) {
    return inst.rd;
  }
  switch (inst.type) {
  case RISC_V::R: {
    if (!issue_IntegerRS(inst, true, false)) {
      return -1;
    }
    break;
  }
  case RISC_V::I: {
    if (inst.opcode == 0b0010011) {
      if (!issue_IntegerRS(inst, false, true)) {
        return -1;
      }
    } else if (inst.opcode == 0b0000011) {
      int n_bytes;
      bool isUnsigned;
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
        nextCPUstate.PCWriteEnable = true;
        return -1;
      }
      if (!issue_Load(inst, n_bytes, isUnsigned)) {
        return -1;
      }
    }
    break;
  }
  case RISC_V::Istar: {
    if (!issue_IntegerRS(inst, false, true)) {
      return -1;
    }
    break;
  }
  case RISC_V::S: {
    int n_bytes;
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
      nextCPUstate.PCWriteEnable = true;
      return -1;
    }
    if (!issue_Store(inst, n_bytes)) {
      return -1;
    }
    break;
  }
  case RISC_V::B:
  case RISC_V::U: {
    if (inst.opcode == 0b0010111) {
      if (!issue_IntegerU(inst, true)) {
        return -1;
      }
    } else if (inst.opcode == 0b0110111) {
      if (!issue_IntegerU(inst, false)) {
        return -1;
      }
    }
    break;
  }
  case RISC_V::J:
  case RISC_V::RV_INVALID:
    break;
  }
  nextCPUstate.PCWriteEnable = true;
  return -1;
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
  if (inst.type == RISC_V::U) {
    if (inst.opcode == 0b0010111)
      return Operation::AUIPC;
    if (inst.opcode == 0b0110111)
      return Operation::LUI;
    return Operation::OP_INVALID;
  }
  return Operation::OP_INVALID;
}

void CPU::apply_B_operation(Instruct inst) {
  auto opNum1 = curCPUstate.REGModule.readReg(inst.rs1);
  auto opNum2 = curCPUstate.REGModule.readReg(inst.rs2);
  auto offset = inst.imm;
  bool taken = false;
  switch (inst.funct3) {
  case 0b000:
    taken = (opNum1 == opNum2);
    break;
  case 0b001:
    taken = (opNum1 != opNum2);
    break;
  case 0b100:
    taken = (opNum1 < opNum2);
    break;
  case 0b101:
    taken = (opNum1 >= opNum2);
    break;
  case 0b110:
    taken = (static_cast<uint32_t>(opNum1) < static_cast<uint32_t>(opNum2));
    break;
  case 0b111:
    taken = (static_cast<uint32_t>(opNum1) >= static_cast<uint32_t>(opNum2));
    break;
  }
  if (taken) {
    nextCPUstate.programCounter = curCPUstate.programCounter + offset;
  }
}

void CPU::apply_J_operation(Instruct inst) {
  nextCPUstate.REGModule.writeReg(inst.rd, curCPUstate.programCounter + 4);
  nextCPUstate.programCounter = curCPUstate.programCounter + inst.imm;
}

void CPU::execute() {
  DPRINT(TOPIC_EXEC, "EXEC: ALUempty=%d\n", curCPUstate.ALUModule.isEmpty());
  if (!nextCPUstate.ALUModule.isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    ReservationStation Execute_RS{};
    Execute_RS.ROB_dest = ~0u >> 1;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = curCPUstate.IntegerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
        }
      }
    }
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = curCPUstate.LoadRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = curCPUstate.StoreRS[i];
      if (!rs.free && rs.qj == -1) {
        if (rs.ROB_dest < Execute_RS.ROB_dest) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      nextCPUstate.ALUModule.ALUExecute(Execute_RS.vj, Execute_RS.vk,
                                        Execute_RS.op, Execute_RS.ROB_dest);
      switch (Execute_RS_type) {
      case 0:
        nextCPUstate.IntegerRS[Execute_RS_index].free = true;
        break;
      case 1:
        nextCPUstate.LoadRS[Execute_RS_index].free = true;
        break;
      case 2:
        nextCPUstate.StoreRS[Execute_RS_index].free = true;
        break;
      }
    }
  }
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (!curCPUstate.MicroStoreRS[i].free &&
        curCPUstate.MicroStoreRS[i].qrs2 == -1) {
      auto index =
          nextCPUstate.LSQModule.getIndex(curCPUstate.MicroStoreRS[i].ROB_dest);
      DPRINT(TOPIC_EXEC, "EXEC microRS->LSQ idx=%d v=%d\n", index,
             curCPUstate.MicroStoreRS[i].vrs2);
      nextCPUstate.LSQModule.writeValue(curCPUstate.MicroStoreRS[i].vrs2,
                                        index);
      nextCPUstate.LSQModule.DataBroadcast(index);
      nextCPUstate.MicroStoreRS[i].free = true;
    }
  }
  auto loadIndex = curCPUstate.LSQModule.LoadDetect();
  DPRINT(TOPIC_EXEC, "EXEC LoadDetect=%d\n", loadIndex);
  nextCPUstate.DataMem.execute();
  if (loadIndex != 0xFFFFFFFF && !curCPUstate.DataMem.isBusy()) {
    MemRequest newRequest{};
    auto entry = curCPUstate.LSQModule.getEntry(loadIndex);
    newRequest.address = entry.address;
    newRequest.isSigned = !entry.isUnsigned;
    newRequest.n_bytes = entry.n_bytes;
    newRequest.op = Operation::Load;
    newRequest.ROBTag = entry.ROBTag;
    nextCPUstate.DataMem.MemPush(newRequest);
    nextCPUstate.LSQModule.setValueState(loadIndex, ValueState::FETCHING);
  }
  for (int i = curCPUstate.LSQModule.getHead();
       i != curCPUstate.LSQModule.getTail(); i = (i + 1) & 0x3F) {
    auto e = curCPUstate.LSQModule.getEntry(i);
    DPRINT(TOPIC_LSQ, "EXEC checkLSQ idx=%d robTag=%d addrR=%d valSt=%d\n", i,
           e.ROBTag, e.isAddressReady, (int)e.valueState);
    if (curCPUstate.LSQModule.isReadyToCommit(i)) {
      auto lsqEntry = curCPUstate.LSQModule.getEntry(i);
      nextCPUstate.ROBModule.writeROB(
          lsqEntry.value, curCPUstate.ROBModule.getIndex(lsqEntry.ROBTag),
          ROBState::Ready);
      DPRINT(TOPIC_EXEC, "EXEC markROB tag=%d val=%d\n", lsqEntry.ROBTag,
             lsqEntry.value);
    }
  }
}

void CPU::writeBack() {
  if (curCPUstate.DataMem.isReady()) {
    auto reply = curCPUstate.DataMem.MemReturn();
    DPRINT(TOPIC_MEM, "WB memReturn op=%d tag=%d val=%d\n", (int)reply.op,
           reply.ROBTag, reply.value);
    if (reply.op == Operation::Load) {
      auto index = curCPUstate.LSQModule.getIndex(reply.ROBTag);
      nextCPUstate.LSQModule.writeValue(reply.value, index);
    }
    nextCPUstate.DataMem.MemPull();
  }
  bool aluValid = !curCPUstate.ALUModule.isEmpty();
  ExecuteResult aluResult =
      aluValid ? curCPUstate.ALUModule.peek() : ExecuteResult{};

  auto lsqCDBDetect = curCPUstate.LSQModule.CDBDetect();
  bool lsqValid = lsqCDBDetect != -1;
  ExecuteResult lsqResult{};
  if (lsqValid) {
    auto lsqEntry = curCPUstate.LSQModule.getEntry(lsqCDBDetect);
    lsqResult.isAddress = false;
    lsqResult.robTag = lsqEntry.ROBTag;
    lsqResult.value = lsqEntry.value;
  }

  CDBOutput cdbOut =
      curCPUstate.CDBModule.arbitrate(aluResult, aluValid, lsqResult, lsqValid);

  DPRINT(TOPIC_WB, "WB bcast tag=%d val=%d isAddr=%d\n", cdbOut.result.robTag,
         cdbOut.result.value, cdbOut.result.isAddress);
  if (!cdbOut.valid)
    return;

  if (cdbOut.aluGranted)
    nextCPUstate.ALUModule.pop();

  auto tag = cdbOut.result.robTag;
  auto value = cdbOut.result.value;
  auto isAddress = cdbOut.result.isAddress;
  if (!isAddress) {
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (curCPUstate.IntegerRS[i].qj == tag) {
        DPRINT(TOPIC_WB, "WB res int[%d] qj=%d->v=%d\n", i, tag, value);
        nextCPUstate.IntegerRS[i].vj = value;
        nextCPUstate.IntegerRS[i].qj = -1;
      }
      if (curCPUstate.IntegerRS[i].qk == tag) {
        DPRINT(TOPIC_WB, "WB res int[%d] qk=%d->v=%d\n", i, tag, value);
        nextCPUstate.IntegerRS[i].vk = value;
        nextCPUstate.IntegerRS[i].qk = -1;
      }
    }
    for (int i = 0; i < LOADRS_CAP; i++) {
      if (curCPUstate.LoadRS[i].qj == tag) {
        nextCPUstate.LoadRS[i].vj = value;
        nextCPUstate.LoadRS[i].qj = -1;
      }
      if (curCPUstate.LoadRS[i].qk == tag) {
        nextCPUstate.LoadRS[i].vk = value;
        nextCPUstate.LoadRS[i].qk = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (curCPUstate.StoreRS[i].qj == tag) {
        nextCPUstate.StoreRS[i].vj = value;
        nextCPUstate.StoreRS[i].qj = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (curCPUstate.MicroStoreRS[i].qrs2 == tag) {
        DPRINT(TOPIC_WB, "WB resolve microRS[%d] v=%d\n", i, value);
        nextCPUstate.MicroStoreRS[i].vrs2 = value;
        nextCPUstate.MicroStoreRS[i].qrs2 = -1;
      }
    }
    nextCPUstate.ROBModule.writeROB(value, curCPUstate.ROBModule.getIndex(tag),
                                    ROBState::Ready);
  } else {
    auto index = curCPUstate.LSQModule.getIndex(tag);
    nextCPUstate.LSQModule.writeAddress(value, index);
    nextCPUstate.LSQModule.AddressBroadcast(index);
  }
}

void CPU::commit() {
  if (curCPUstate.ROBModule.peek().state != ROBState::Ready)
    return;
  auto rob_entry = nextCPUstate.ROBModule.pop();
  DPRINT(TOPIC_COMMIT, "COMMIT tag=%d type=%d val=%d dest=%d\n", rob_entry.tag,
         (int)rob_entry.type, rob_entry.value, rob_entry.dest);
  if (rob_entry.type == ROBType::REGISTER) {
    nextCPUstate.REGModule.writeReg(rob_entry.dest, rob_entry.value);
    if (curCPUstate.REGModule.readRAT(rob_entry.dest) == rob_entry.tag) {
      nextCPUstate.REGModule.setRAT(rob_entry.dest, 0xFFFFFFFF);
    }
  }
  if (curCPUstate.LSQModule.peek().ROBTag == rob_entry.tag) {
    auto entry = nextCPUstate.LSQModule.pop();
    if (rob_entry.type == ROBType::STORE) {
      MemRequest newRequest{};
      newRequest.address = entry.address;
      newRequest.value = entry.value;
      newRequest.isSigned = !entry.isUnsigned;
      newRequest.n_bytes = entry.n_bytes;
      newRequest.op = Operation::Store;
      newRequest.ROBTag = entry.ROBTag;
      nextCPUstate.DataMem.MemPush(newRequest);
    }
  }
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
    nextCPUstate.REGModule.resetX0();
    if (nextCPUstate.programCounter == curCPUstate.programCounter &&
        nextCPUstate.PCWriteEnable)
      nextCPUstate.programCounter += 4;
    if (finish) {
      std::cout << std::dec << (curCPUstate.REGModule.readReg(result) & 0xFF)
                << std::endl;
      std::cout << "Clock cycles: " << clock << std::endl;
    }
    curCPUstate = nextCPUstate;
    ++clock;
  }
}