#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
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
    auto prediction = BPModule.predict(programCounter);
    auto predictedPC =
        prediction.taken ? prediction.predictPC : programCounter + 4;
    auto ckpt = BPModule.snapshotCheckPoint();
    auto opcode = raw_inst & 0x7F;
    auto rd = (raw_inst >> 7) & 0x1F;
    auto rs1 = (raw_inst >> 15) & 0x1F;
    auto imm_i = (raw_inst >> 20) & 0xFFF;
    if (opcode == 0b1101111 && rd == 1) {
      if (!CPUstate.BPModule.RAS_full())
        CPUstate.BPModule.RAS_push(programCounter + 4);
    } else if (opcode == 0b1100111 && rd == 0 && rs1 == 1 && imm_i == 0) {
      if (!CPUstate.BPModule.RAS_empty())
        predictedPC = CPUstate.BPModule.RAS_pop();
    }
    if (opcode == 0b1100011)
      CPUstate.BPModule.shiftGHR(prediction.taken);
    else if (opcode == 0b1101111 || opcode == 0b1100111)
      CPUstate.BPModule.shiftGHR(true);
    CPUstate.INQModule.push(raw_inst, programCounter, predictedPC, ckpt);
    CPUstate.programCounter = predictedPC;
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

int CPU::issue_IntegerRS(Instruct inst, bool has_rs2, bool imm_as_vk,
                                 bool isControl) {
  if (IntegerRSModule.isIntegerRSFull() || ROBModule.isFull()) {
    return -1;
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
    if (PRFModule.isReady(op1.phyRegIndex)) {
      IntegerRS.vj = PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op1.phyRegIndex);
      if (bypass.valid) {
        IntegerRS.vj = bypass.value;
      } else {
        IntegerRS.qj = op1.phyRegIndex;
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
      if (PRFModule.isReady(op2.phyRegIndex)) {
        IntegerRS.vk = PRFModule.getValue(op2.phyRegIndex);
      } else {
        auto bypass = CDBBypass(op2.phyRegIndex);
        if (bypass.valid) {
          IntegerRS.vk = bypass.value;
        } else {
          IntegerRS.qk = op2.phyRegIndex;
        }
      }
    }
  }
  int Physical_regNum = -1;
  if (inst.allocDest && !PRFModule.isFreeListEmpty()) {
    Physical_regNum = CPUstate.PRFModule.pop();
    CPUstate.REGModule.setRAT_PRF(destination, Physical_regNum);
  }
  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.pc = inst.pc;
  newROB.predictedPC = INQModule.headPredictedPC();
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.oldPhy =
      inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  if (isControl) {
    newROB.type = ROBType::LINK;
    newROB.ckpt.BPsnapshot = INQModule.headBPSnapshot();
    auto prfSnap = REGModule.snapshotRAT_PRF();
    memcpy(newROB.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
           sizeof(newROB.ckpt.RATsnapshot.RAT_snapshot));
    if (inst.allocDest)
      newROB.ckpt.RATsnapshot.RAT_snapshot[destination] = Physical_regNum;
    newROB.ckpt.flHeadSeqCkpt =
        PRFModule.getHeadSeq() + (inst.allocDest ? 1 : 0);
    if (Physical_regNum >= 0) {
      CPUstate.PRFModule.write(Physical_regNum, inst.pc + 4);
      if (debug::enabled(debug::TOPIC_PRF))
        debug::print("PRF link P%d = %d (pc+4)\n", Physical_regNum,
                     inst.pc + 4);
    }
  }
  int robIndex = CPUstate.ROBModule.push(newROB);
  IntegerRS.robIndex = robIndex;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (IntegerRSModule.IntegerRS[i].free) {
      CPUstate.IntegerRSModule.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return robIndex;
}

int CPU::issue_UandJ(Instruct inst, bool has_PC, bool isControl) {
  if (IntegerRSModule.isIntegerRSFull() || ROBModule.isFull()) {
    return -1;
  }
  ReservationStation IntegerRS{};
  IntegerRS.free = false;
  IntegerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  if (has_PC) {
    IntegerRS.vj = inst.pc;
  }
  IntegerRS.vk = inst.imm;
  int Physical_regNum = -1;
  if (inst.allocDest && !PRFModule.isFreeListEmpty()) {
    Physical_regNum = CPUstate.PRFModule.pop();
    CPUstate.REGModule.setRAT_PRF(destination, Physical_regNum);
  }
  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.pc = inst.pc;
  newROB.predictedPC = INQModule.headPredictedPC();
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.oldPhy =
      inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  if (isControl) {
    newROB.type = ROBType::LINK;
    newROB.ckpt.BPsnapshot = INQModule.headBPSnapshot();
    auto prfSnap = REGModule.snapshotRAT_PRF();
    memcpy(newROB.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
           sizeof(newROB.ckpt.RATsnapshot.RAT_snapshot));
    if (inst.allocDest)
      newROB.ckpt.RATsnapshot.RAT_snapshot[destination] = Physical_regNum;
    newROB.ckpt.flHeadSeqCkpt =
        PRFModule.getHeadSeq() + (inst.allocDest ? 1 : 0);
    if (Physical_regNum >= 0) {
      CPUstate.PRFModule.write(Physical_regNum, inst.pc + 4);
      if (debug::enabled(debug::TOPIC_PRF))
        debug::print("PRF link P%d = %d (pc+4)\n", Physical_regNum,
                     inst.pc + 4);
    }
  }
  int robIndex = CPUstate.ROBModule.push(newROB);
  IntegerRS.robIndex = robIndex;
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (IntegerRSModule.IntegerRS[i].free) {
      CPUstate.IntegerRSModule.IntegerRS[i] = IntegerRS;
      break;
    }
  }
  return robIndex;
}

int CPU::issue_B(Instruct inst) {
  if (BranchRSModule.isBranchRSFull() || ROBModule.isFull()) {
    return -1;
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
    if (PRFModule.isReady(op1.phyRegIndex)) {
      BranchRS.vj = PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op1.phyRegIndex);
      if (bypass.valid) {
        BranchRS.vj = bypass.value;
      } else {
        BranchRS.qj = op1.phyRegIndex;
      }
    }
  }
  auto op2 = REGModule.readOperand(regNum2);
  if (op2.ready) {
    BranchRS.vk = op2.value;
  } else {
    if (PRFModule.isReady(op2.phyRegIndex)) {
      BranchRS.vk = PRFModule.getValue(op2.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op2.phyRegIndex);
      if (bypass.valid) {
        BranchRS.vk = bypass.value;
      } else {
        BranchRS.qk = op2.phyRegIndex;
      }
    }
  }
  ROBEntry newROB(ROBType::BRANCH);
  newROB.pc = inst.pc;
  newROB.predictedPC = INQModule.headPredictedPC();
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.ckpt.BPsnapshot = INQModule.headBPSnapshot();
  auto prfSnap = REGModule.snapshotRAT_PRF();
  memcpy(newROB.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
         sizeof(newROB.ckpt.RATsnapshot.RAT_snapshot));
  newROB.ckpt.flHeadSeqCkpt = PRFModule.getHeadSeq();
  int robIndex = CPUstate.ROBModule.push(newROB);
  BranchRS.robIndex = robIndex;
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (BranchRSModule.BranchRS[i].free) {
      CPUstate.BranchRSModule.BranchRS[i] = BranchRS;
      break;
    }
  }
  return robIndex;
}

int CPU::issue_Load(Instruct inst, int n_bytes, bool isUnsigned) {
  if (LoadRSModule.isLoadRSFull() || ROBModule.isFull() || LSQModule.isFull()) {
    return -1;
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
    if (PRFModule.isReady(op1.phyRegIndex)) {
      LoadRS.vj = PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op1.phyRegIndex);
      if (bypass.valid) {
        LoadRS.vj = bypass.value;
      } else {
        LoadRS.qj = op1.phyRegIndex;
      }
    }
  }
  LoadRS.vk = inst.imm;
  int Physical_regNum = -1;
  if (inst.allocDest && !PRFModule.isFreeListEmpty()) {
    Physical_regNum = CPUstate.PRFModule.pop();
    CPUstate.REGModule.setRAT_PRF(destination, Physical_regNum);
  }
  ROBEntry newROB(ROBType::REGISTER);
  newROB.dest = destination;
  newROB.oldPhy =
      inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  int robIndex = CPUstate.ROBModule.push(newROB);
  LoadRS.robIndex = robIndex;
  CPUstate.LSQModule.pushLoad(robIndex, CPUstate.ROBModule.getSeq(robIndex),
                              n_bytes, isUnsigned);
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (LoadRSModule.LoadRS[i].free) {
      CPUstate.LoadRSModule.LoadRS[i] = LoadRS;
      break;
    }
  }
  return robIndex;
}

int CPU::issue_Store(Instruct inst, int n_bytes) {

  if (StoreAddressRSModule.isStoreAddressRSFull() ||
      StoreValueRSModule.isStoreValueRSFull() || ROBModule.isFull() ||
      LSQModule.isFull()) {
    return -1;
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
    if (PRFModule.isReady(op1.phyRegIndex)) {
      StoreRS.vj = PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op1.phyRegIndex);
      if (bypass.valid) {
        StoreRS.vj = bypass.value;
      } else {
        StoreRS.qj = op1.phyRegIndex;
      }
    }
  }
  StoreRS.vk = inst.imm;

  StoreValueReservationStation MicroRS{};
  MicroRS.free = false;
  auto op2 = REGModule.readOperand(regNum2);
  if (op2.ready) {
    MicroRS.vrs2 = op2.value;
  } else {
    if (PRFModule.isReady(op2.phyRegIndex)) {
      MicroRS.vrs2 = PRFModule.getValue(op2.phyRegIndex);
    } else {
      auto bypass = CDBBypass(op2.phyRegIndex);
      if (bypass.valid) {
        MicroRS.vrs2 = bypass.value;
      } else {
        MicroRS.qrs2 = op2.phyRegIndex;
      }
    }
  }

  ROBEntry newROB(ROBType::STORE);
  int robIndex = CPUstate.ROBModule.push(newROB);
  StoreRS.robIndex = robIndex;
  MicroRS.robIndex = robIndex;
  CPUstate.LSQModule.pushStore(robIndex, CPUstate.ROBModule.getSeq(robIndex),
                               n_bytes);
  for (int i = 0; i < STORERS_CAP; i++) {
    if (StoreAddressRSModule.StoreAddressRS[i].free) {
      CPUstate.StoreAddressRSModule.StoreAddressRS[i] = StoreRS;
      break;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (StoreValueRSModule.StoreValueRS[i].free) {
      CPUstate.StoreValueRSModule.StoreValueRS[i] = MicroRS;
      break;
    }
  }
  return robIndex;
}

void CPU::issue() {
  if (!squashDetect.needSquash) {
    int res = -1;
    if (!INQModule.isEmpty() && INQModule.headDecoded()) {
      Instruct inst = INQModule.headNinst();
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
            newROB.isCommitReady = true;
            int haltIndex = CPUstate.ROBModule.push(newROB);
            res = haltIndex;
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
        res = -1;
        break;
      }
      }
    }
    if (res != -1)
      CPUstate.INQModule.pop();
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
    ReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = IntegerRSModule.IntegerRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          foundAny = true;
        } else if (ROBModule.getSeq(rs.robIndex) <
                   ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = ROBModule.getSeq(Execute_RS.robIndex);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash && execSeq < squashDetect.SquashSeq)) {
        CPUstate.ALUModule.push(Execute_RS.vj, Execute_RS.vk, Execute_RS.op,
                                Execute_RS.robIndex, execSeq,
                                isControlOp(Execute_RS.op));
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].free = true;
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].qj = -1;
        CPUstate.IntegerRSModule.IntegerRS[Execute_RS_index].qk = -1;
      }
    }
  }
  // AGU execute
  if (!AGUModule.isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    ReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = LoadRSModule.LoadRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
          foundAny = true;
        } else if (ROBModule.getSeq(rs.robIndex) <
                   ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 1;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = StoreAddressRSModule.StoreAddressRS[i];
      if (!rs.free && rs.qj == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
          foundAny = true;
        } else if (ROBModule.getSeq(rs.robIndex) <
                   ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 2;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = ROBModule.getSeq(Execute_RS.robIndex);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash && execSeq < squashDetect.SquashSeq)) {
        CPUstate.AGUModule.push(Execute_RS.vj, Execute_RS.vk, Execute_RS.op,
                                Execute_RS.robIndex, execSeq);
        if (Execute_RS_type == 1) {
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].free = true;
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].qj = -1;
          CPUstate.LoadRSModule.LoadRS[Execute_RS_index].qk = -1;
        } else {
          CPUstate.StoreAddressRSModule.StoreAddressRS[Execute_RS_index].free =
              true;
          CPUstate.StoreAddressRSModule.StoreAddressRS[Execute_RS_index].qj =
              -1;
        }
      }
    }
  }
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (!StoreValueRSModule.StoreValueRS[i].free &&
        StoreValueRSModule.StoreValueRS[i].qrs2 == -1) {
      uint64_t microSeq =
          ROBModule.getSeq(StoreValueRSModule.StoreValueRS[i].robIndex);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash && microSeq < squashDetect.SquashSeq)) {
        auto index = LSQModule.getIndexBySeq(microSeq);
        if (index >= 0) {
          auto plan = LSQModule.planDataForward(
              index, StoreValueRSModule.StoreValueRS[i].vrs2);
          if (debug::enabled(debug::TOPIC_LSQ))
            debug::print("LSQ store value seq=%llu -> LSQ[%d] = %d\n",
                         static_cast<unsigned long long>(microSeq), index,
                         StoreValueRSModule.StoreValueRS[i].vrs2);
          CPUstate.LSQModule.writeValue(StoreValueRSModule.StoreValueRS[i].vrs2,
                                        index);
          CPUstate.LSQModule.applyStoreToLoadForward(plan);
        }
        CPUstate.StoreValueRSModule.StoreValueRS[i].free = true;
        CPUstate.StoreValueRSModule.StoreValueRS[i].qrs2 = -1;
      }
    }
  }
  // BRU execute
  if (!BRUModule.isFull()) {
    int Execute_RS_index = 0xFFFFFFFF;
    int Execute_RS_type = -1;
    BranchReservationStation Execute_RS{};
    bool foundAny = false;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = BranchRSModule.BranchRS[i];
      if (!rs.free && rs.qj == -1 && rs.qk == -1) {
        if (!foundAny) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
          foundAny = true;
        } else if (ROBModule.getSeq(rs.robIndex) <
                   ROBModule.getSeq(Execute_RS.robIndex)) {
          Execute_RS = rs;
          Execute_RS_index = i;
          Execute_RS_type = 0;
        }
      }
    }
    if (Execute_RS_index != 0xFFFFFFFF) {
      uint64_t execSeq = ROBModule.getSeq(Execute_RS.robIndex);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash && execSeq < squashDetect.SquashSeq)) {
        CPUstate.BRUModule.BRUExecute(
            Execute_RS.vj, Execute_RS.vk, Execute_RS.pc, Execute_RS.imm,
            Execute_RS.op, Execute_RS.robIndex, execSeq);
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].free = true;
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].qj = -1;
        CPUstate.BranchRSModule.BranchRS[Execute_RS_index].qk = -1;
      }
    }
  }
  // MEM execute
  CPUstate.DataMem.execute();
  // LSQ execute
  bool memBusy = DataMem.isBusy();
  if (!memBusy && !LSQModule.isEmpty() && !LSQModule.isHeadLoad()) {
    auto storeSeq = LSQModule.headRobSeq();
    bool committed = ROBModule.isEmpty() || storeSeq < ROBModule.headSeq();
    bool atHeadReady =
        !committed &&
        LSQModule.getRobIndex(LSQModule.getHead()) == ROBModule.getHead() &&
        ROBModule.isCommitReadyAt(LSQModule.getRobIndex(LSQModule.getHead()));
    if (committed || atHeadReady) {
      MemRequest newRequest{};
      newRequest.address = LSQModule.getAddress(LSQModule.getHead());
      newRequest.value = LSQModule.getValue(LSQModule.getHead());
      newRequest.isSigned = !LSQModule.getIsUnsigned(LSQModule.getHead());
      newRequest.n_bytes = LSQModule.getNBytes(LSQModule.getHead());
      newRequest.op = Operation::Store;
      newRequest.robIndex = LSQModule.getRobIndex(LSQModule.getHead());
      newRequest.robSeq = storeSeq;
      if (CPUstate.DataMem.MemPush(newRequest)) {
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("LSQ store dispatch seq=%llu @%u <- %d\n",
                       static_cast<unsigned long long>(newRequest.robSeq),
                       newRequest.address, newRequest.value);
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
    newRequest.address = LSQModule.getAddress(loadIndex);
    newRequest.isSigned = !LSQModule.getIsUnsigned(loadIndex);
    newRequest.n_bytes = LSQModule.getNBytes(loadIndex);
    newRequest.op = Operation::Load;
    newRequest.robIndex = LSQModule.getRobIndex(loadIndex);
    newRequest.robSeq = LSQModule.getRobSeq(loadIndex);
    if (!squashDetect.needSquash ||
        (squashDetect.needSquash &&
         newRequest.robSeq < squashDetect.SquashSeq)) {
      if (CPUstate.DataMem.MemPush(newRequest)) {
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("LSQ load dispatch seq=%llu @%u\n",
                       static_cast<unsigned long long>(newRequest.robSeq),
                       newRequest.address);
        CPUstate.LSQModule.setValueState(loadIndex, ValueState::FETCHING);
      }
    }
  }
  auto head = LSQModule.getHead();
  auto tail = LSQModule.getTail();
  for (int i = head; i != ((head + (LSQ_CAP >> 3)) & 0x3F);
       i = (i + 1) & 0x3F) {
    if (i == tail)
      break;
    if (LSQModule.isReadyToCommit(i)) {
      auto lsqRobIndex = LSQModule.getRobIndex(i);
      auto lsqSeq = LSQModule.getRobSeq(i);
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash && lsqSeq < squashDetect.SquashSeq)) {
        if (!ROBModule.isEmpty() && lsqSeq >= ROBModule.headSeq()) {
          if (debug::enabled(debug::TOPIC_LSQ))
            debug::print("LSQ ready seq=%llu -> ROB[%d] = %d\n",
                         static_cast<unsigned long long>(lsqSeq), lsqRobIndex,
                         LSQModule.getValue(i));
          CPUstate.ROBModule.setROBCommitReady(lsqRobIndex);
          int newPhy = ROBModule.getNewPhy(lsqRobIndex);
          if (newPhy >= 0) {
            CPUstate.PRFModule.write(newPhy, LSQModule.getValue(i));
            if (debug::enabled(debug::TOPIC_PRF))
              debug::print("PRF write P%d = %d (lsq)\n", newPhy,
                           LSQModule.getValue(i));
          }
        }
      }
    }
  }
}

void CPU::CDBBroadcast(int robIndex, int value) {
  int phy = ROBModule.getNewPhy(robIndex);
  if (phy < 0)
    return; // no dest register: nothing to broadcast
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (!IntegerRSModule.IntegerRS[i].free &&
        IntegerRSModule.IntegerRS[i].qj == phy) {
      CPUstate.IntegerRSModule.IntegerRS[i].vj = value;
      CPUstate.IntegerRSModule.IntegerRS[i].qj = -1;
    }
    if (!IntegerRSModule.IntegerRS[i].free &&
        IntegerRSModule.IntegerRS[i].qk == phy) {
      CPUstate.IntegerRSModule.IntegerRS[i].vk = value;
      CPUstate.IntegerRSModule.IntegerRS[i].qk = -1;
    }
  }
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (!LoadRSModule.LoadRS[i].free && LoadRSModule.LoadRS[i].qj == phy) {
      CPUstate.LoadRSModule.LoadRS[i].vj = value;
      CPUstate.LoadRSModule.LoadRS[i].qj = -1;
    }
    if (!LoadRSModule.LoadRS[i].free && LoadRSModule.LoadRS[i].qk == phy) {
      CPUstate.LoadRSModule.LoadRS[i].vk = value;
      CPUstate.LoadRSModule.LoadRS[i].qk = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!StoreAddressRSModule.StoreAddressRS[i].free &&
        StoreAddressRSModule.StoreAddressRS[i].qj == phy) {
      CPUstate.StoreAddressRSModule.StoreAddressRS[i].vj = value;
      CPUstate.StoreAddressRSModule.StoreAddressRS[i].qj = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!StoreValueRSModule.StoreValueRS[i].free &&
        StoreValueRSModule.StoreValueRS[i].qrs2 == phy) {
      CPUstate.StoreValueRSModule.StoreValueRS[i].vrs2 = value;
      CPUstate.StoreValueRSModule.StoreValueRS[i].qrs2 = -1;
    }
  }
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (!BranchRSModule.BranchRS[i].free &&
        BranchRSModule.BranchRS[i].qj == phy) {
      CPUstate.BranchRSModule.BranchRS[i].vj = value;
      CPUstate.BranchRSModule.BranchRS[i].qj = -1;
    }
    if (!BranchRSModule.BranchRS[i].free &&
        BranchRSModule.BranchRS[i].qk == phy) {
      CPUstate.BranchRSModule.BranchRS[i].vk = value;
      CPUstate.BranchRSModule.BranchRS[i].qk = -1;
    }
  }
}

CDBBypassResult CPU::CDBBypass(int phy) const {
  CDBBypassResult out;
  if (cdbArbiter.valid && !cdbArbiter.result.isControl &&
      ROBModule.getNewPhy(cdbArbiter.result.robIndex) == phy) {
    out.valid = true;
    out.value = cdbArbiter.result.value;
  }
  return out;
}

void CPU::writeBack() {
  // DMEM write back to the Load
  if (DataMem.isReady()) {
    auto reply = DataMem.MemReturn();
    if (reply.op == Operation::Load &&
        (!squashDetect.needSquash ||
         (squashDetect.needSquash && reply.robSeq < squashDetect.SquashSeq))) {
      auto index = LSQModule.getIndexBySeq(reply.robSeq);
      if (index >= 0) {
        if (debug::enabled(debug::TOPIC_MEM))
          debug::print("MEM load @%u <- %d\n", reply.address, reply.value);
        CPUstate.LSQModule.writeValue(reply.value, index);
      }
    }
    CPUstate.DataMem.MemPull();
  }
  // BRU write back: if predicted wrong, send the flush signal
  SquashInfo BranchSquash;
  if (!BRUModule.isEmpty()) {
    int index = BRUModule.headRobIndex();
    uint64_t brRobSeq = BRUModule.headRobSeq();
    int pcResult = BRUModule.headPCResult();
    int pcFrom = BRUModule.headPCFrom();
    if (index >= 0) {
      ++branchTotal;
      if (pcResult == ROBModule.getPredictedPC(index)) {
        ++branchCorrect;
      }
    }
    if (index >= 0 &&
        (!squashDetect.needSquash ||
         (squashDetect.needSquash && brRobSeq < squashDetect.SquashSeq))) {
      auto actualPC = pcResult;
      auto taken = actualPC != pcFrom + 4;
      if (actualPC != ROBModule.getPredictedPC(index)) {
        if (debug::enabled(debug::TOPIC_BRANCH))
          debug::print("squash seq=%llu pc=%u (from %u)\n",
                       static_cast<unsigned long long>(brRobSeq), actualPC,
                       pcFrom);
        BranchSquash.needSquash = true;
        BranchSquash.SquashPC = actualPC;
        BranchSquash.SquashIndex = index;
        BranchSquash.SquashSeq = brRobSeq;
      }
      CPUstate.BPModule.update(pcFrom, taken, pcResult,
                               ROBModule.getRASCkpt(index).GHR_snapshot);
      CPUstate.ROBModule.setROBCommitReady(index);
    }
    CPUstate.BRUModule.remove(brRobSeq);
  }

  // AGU writeBack: address result -> LSQ directly
  if (!AGUModule.isEmpty()) {
    auto aguRobSeq = AGUModule.headRobSeq();
    if (!squashDetect.needSquash ||
        (squashDetect.needSquash && aguRobSeq < squashDetect.SquashSeq)) {
      auto index = LSQModule.getIndexBySeq(aguRobSeq);
      if (index >= 0) {
        auto value = AGUModule.headValue();
        auto plan = LSQModule.planAddressForward(index, value);
        CPUstate.LSQModule.writeAddress(static_cast<uint32_t>(value), index);
        CPUstate.LSQModule.applyStoreToLoadForward(plan);
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("AGU addr seq=%llu -> LSQ[%d] = %u\n",
                       static_cast<unsigned long long>(aguRobSeq), index,
                       static_cast<uint32_t>(value));
      }
    }
    CPUstate.AGUModule.remove(aguRobSeq);
  }

  CDBOutput cdbOut = cdbArbiter;
  if (!cdbOut.valid) {
    if (BranchSquash.needSquash) {
      CPUstate.flushArbiter.receive(BranchSquash);
    }
    return;
  }

  if (cdbOut.aluGranted)
    CPUstate.ALUModule.remove(cdbOut.result.robSeq);
  if (squashDetect.needSquash &&
      cdbOut.result.robSeq > squashDetect.SquashSeq) {
    if (BranchSquash.needSquash) {
      CPUstate.flushArbiter.receive(BranchSquash);
    }
    return;
  }
  auto robIndex = cdbOut.result.robIndex;
  auto robSeq = cdbOut.result.robSeq;
  auto isControl = cdbOut.result.isControl;
  SquashInfo JumpSquash;
  if (!isControl) {
    auto value = cdbOut.result.value;
    CDBBroadcast(robIndex, value);
    if (!ROBModule.isEmpty() && robSeq >= ROBModule.headSeq()) {
      CPUstate.ROBModule.setROBCommitReady(robIndex);
    }
    if (cdbOut.lsqGranted) {
      auto lsqIndex = LSQModule.getIndexBySeq(robSeq);
      if (lsqIndex >= 0)
        CPUstate.LSQModule.setCDBBroadcast(lsqIndex);
    }
    int newPhy = ROBModule.getNewPhy(robIndex);
    if (newPhy >= 0) {
      CPUstate.PRFModule.write(newPhy, value);
      if (debug::enabled(debug::TOPIC_PRF)) {
        debug::print("PRF write P%d = %d (alu)\n", newPhy, value);
        if (PRFModule.isReady(newPhy) && PRFModule.getValue(newPhy) != value)
          debug::print("PRF mismatch P%d: rob=%d prf=%d\n", newPhy, value,
                       PRFModule.getValue(newPhy));
      }
    }
  } else if (isControl) {
    if (!ROBModule.isEmpty() && robSeq >= ROBModule.headSeq()) {
      const auto pc = static_cast<uint32_t>(cdbOut.result.value);
      const auto value = PRFModule.getValue(ROBModule.getNewPhy(robIndex));

      CDBBroadcast(robIndex, value);
      ++branchTotal;
      if (pc == ROBModule.getPredictedPC(robIndex)) {
        ++branchCorrect;
      }
      CPUstate.BPModule.update(ROBModule.getPC(robIndex), true,
                               static_cast<int32_t>(pc),
                               ROBModule.getRASCkpt(robIndex).GHR_snapshot);
      if (pc != ROBModule.getPredictedPC(robIndex)) {
        if (debug::enabled(debug::TOPIC_BRANCH))
          debug::print("squash seq=%llu pc=%u (jalr)\n",
                       static_cast<unsigned long long>(robSeq), pc);
        JumpSquash.needSquash = true;
        JumpSquash.SquashPC = pc;
        JumpSquash.SquashIndex = robIndex;
        JumpSquash.SquashSeq = robSeq;
      }
      CPUstate.ROBModule.setROBCommitReady(robIndex);
    }
  }
  if (BranchSquash.needSquash && JumpSquash.needSquash) {
    CPUstate.flushArbiter.receive(BranchSquash.SquashSeq < JumpSquash.SquashSeq
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
    CPUstate.ROBModule.flush(squashDetect.SquashIndex);
    return;
  }
  uint8_t cur = LSQModule.getHead();
  if (cur != LSQModule.getTail() && LSQModule.isHeadLoad() &&
      (ROBModule.isEmpty() || LSQModule.headRobSeq() < ROBModule.headSeq())) {
    CPUstate.LSQModule.pop();
  }
  if (ROBModule.isEmpty() || !ROBModule.isHeadCommitReady())
    return;
  int headIdx = ROBModule.getHead();
  auto rob_entry = ROBModule.peek();
  rob_entry = CPUstate.ROBModule.pop();
  if (rob_entry.halt) {
    CPUstate.haltCommitted = true;
    CPUstate.haltRd = rob_entry.dest;
  } else if (rob_entry.type == ROBType::REGISTER ||
             rob_entry.type == ROBType::LINK) {
    int newPhy = ROBModule.getNewPhy(headIdx);
    int oldPhy = ROBModule.getOldPhy(headIdx);
    auto value = PRFModule.getValue(newPhy);
    if (debug::enabled(debug::TOPIC_COMMIT))
      debug::print("commit seq=%llu dest=%d val=%d\n",
                   static_cast<unsigned long long>(rob_entry.seq),
                   rob_entry.dest, value);
    CPUstate.REGModule.writeReg(rob_entry.dest, value);
    if (oldPhy >= 0)
      CPUstate.PRFModule.push(oldPhy);
  }
}
void CPU::flush() {
  if (squashDetect.needSquash) {
    // 1. clear the wrong RS
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!IntegerRSModule.IntegerRS[i].free &&
          ROBModule.getSeq(IntegerRSModule.IntegerRS[i].robIndex) >
              squashDetect.SquashSeq) {
        CPUstate.IntegerRSModule.IntegerRS[i].free = true;
        CPUstate.IntegerRSModule.IntegerRS[i].qj = -1;
        CPUstate.IntegerRSModule.IntegerRS[i].qk = -1;
      }
    }
    for (int i = 0; i < LOADRS_CAP; i++) {
      if (!LoadRSModule.LoadRS[i].free &&
          ROBModule.getSeq(LoadRSModule.LoadRS[i].robIndex) >
              squashDetect.SquashSeq) {
        CPUstate.LoadRSModule.LoadRS[i].free = true;
        CPUstate.LoadRSModule.LoadRS[i].qj = -1;
        CPUstate.LoadRSModule.LoadRS[i].qk = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!StoreAddressRSModule.StoreAddressRS[i].free &&
          ROBModule.getSeq(StoreAddressRSModule.StoreAddressRS[i].robIndex) >
              squashDetect.SquashSeq) {
        CPUstate.StoreAddressRSModule.StoreAddressRS[i].free = true;
        CPUstate.StoreAddressRSModule.StoreAddressRS[i].qj = -1;
      }
    }
    for (int i = 0; i < BRANCHRS_CAP; i++) {
      if (!BranchRSModule.BranchRS[i].free &&
          ROBModule.getSeq(BranchRSModule.BranchRS[i].robIndex) >
              squashDetect.SquashSeq) {
        CPUstate.BranchRSModule.BranchRS[i].free = true;
        CPUstate.BranchRSModule.BranchRS[i].qj = -1;
        CPUstate.BranchRSModule.BranchRS[i].qk = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!StoreValueRSModule.StoreValueRS[i].free &&
          ROBModule.getSeq(StoreValueRSModule.StoreValueRS[i].robIndex) >
              squashDetect.SquashSeq) {
        CPUstate.StoreValueRSModule.StoreValueRS[i].free = true;
        CPUstate.StoreValueRSModule.StoreValueRS[i].qrs2 = -1;
      }
    }
    // 2. clear the wrong LSQ
    CPUstate.LSQModule.flush(
        ROBModule.getLsqTailSnapshot(squashDetect.SquashIndex));
    // 3. clear the wrong RAT_PRF 
    // only modified by the issue instructions after the control inst
    if (squashDetect.SquashIndex >= 0) {
      RATSnapshot snapP;
      memcpy(snapP.RAT_snapshot,
             ROBModule.getRATPrfCkpt(squashDetect.SquashIndex),
             sizeof(snapP.RAT_snapshot));
      CPUstate.REGModule.restoreRAT_PRF(snapP);
    }
    // 4. clear the wrong ALU outputBuffer
    CPUstate.ALUModule.flush(squashDetect.SquashSeq);
    // 5. clear the wrong AGU outputBuffer
    CPUstate.AGUModule.flush(squashDetect.SquashSeq);
    // 6. clear the wrong BRU outputBuffer
    CPUstate.BRUModule.flush(squashDetect.SquashSeq);
    // 7. clear the old flushArbiter elements
    CPUstate.flushArbiter.clear(squashDetect.SquashSeq);
    // 8. clear the wrong RAS
    auto index = squashDetect.SquashIndex;
    if (index >= 0) {
      CPUstate.BPModule.recoverCheckPoint(ROBModule.getRASCkpt(index));
    }
    // 9. clear the wrong PRF free list
    if (index >= 0) {
      auto ckptHead = ROBModule.getFlHeadSeqCkpt(index);
      CPUstate.PRFModule.restoreHead(ckptHead);
    }
  }
}
void CPU::read() {
  memcpy(&IntegerRSModule, &CPUstate.IntegerRSModule, sizeof(IntegerRSModule));
  memcpy(&StoreAddressRSModule, &CPUstate.StoreAddressRSModule,
         sizeof(StoreAddressRSModule));
  memcpy(&StoreValueRSModule, &CPUstate.StoreValueRSModule,
         sizeof(StoreValueRSModule));
  memcpy(&LoadRSModule, &CPUstate.LoadRSModule, sizeof(LoadRSModule));
  memcpy(&BranchRSModule, &CPUstate.BranchRSModule, sizeof(BranchRSModule));
  memcpy(&REGModule, &CPUstate.REGModule, sizeof(REGModule));
  memcpy(&ROBModule, &CPUstate.ROBModule, sizeof(ROBModule));
  memcpy(&ALUModule, &CPUstate.ALUModule, sizeof(ALUModule));
  memcpy(&AGUModule, &CPUstate.AGUModule, sizeof(AGUModule));
  memcpy(&BRUModule, &CPUstate.BRUModule, sizeof(BRUModule));
  memcpy(&LSQModule, &CPUstate.LSQModule, sizeof(LSQModule));
  memcpy(&INQModule, &CPUstate.INQModule, sizeof(INQModule));
  memcpy(&PRFModule, &CPUstate.PRFModule, sizeof(PRFModule));
  memcpy(&BPModule, &CPUstate.BPModule, sizeof(BPModule));
  memcpy(&flushArbiter, &CPUstate.flushArbiter, sizeof(flushArbiter));
  DataMem.snapshotFrom(CPUstate.DataMem);
  programCounter = CPUstate.programCounter;
  haltFetched = CPUstate.haltFetched;
  haltCommitted = CPUstate.haltCommitted;
  haltRd = CPUstate.haltRd;
  squashDetect = CPUstate.flushArbiter.arbitResult();
  cdbArbiter = CDBArbiter::arbitrate(ALUModule, LSQModule, squashDetect);
}

bool CPU::checkPRFInvariant() const {
  
  uint32_t bitmap[PRF_CAP / 32] = {};
  uint32_t count = 0;
  auto mark = [&](int phy) {
    assert(phy >= 0 && phy < PRF_CAP);
    assert(!((bitmap[phy >> 5] >> (phy & 31)) & 1u));
    bitmap[phy >> 5] |= 1u << (phy & 31);
    ++count;
  };
  mark(0);
  for (uint32_t s = CPUstate.PRFModule.getHeadSeq();
       s != CPUstate.PRFModule.getTailSeq(); ++s)
    mark(CPUstate.PRFModule.getFreeListSlot(s));
  for (int r = 1; r < REGISTER_CAP; ++r)
    mark(CPUstate.REGModule.readRAT_PRF(r));
  for (int i = CPUstate.ROBModule.getHead(); i != CPUstate.ROBModule.getTail();
       i = (i + 1) & (ROB_CAP - 1)) {
    int old = CPUstate.ROBModule.getOldPhy(i);
    if (old >= 0)
      mark(old);
  }
  return count == PRF_CAP;
}

void CPU::run() {
  bool finish = false;
  uint64_t clock = 0;
  while (!finish) {
    read();
    fetch();
    issue();
    writeBack();
    execute();
    commit();
    flush();
    decode();
    CPUstate.REGModule.resetX0();
    assert(checkPRFInvariant());
    ++clock;
    finish = haltCommitted && INQModule.isEmpty() && ROBModule.isEmpty();
  }
  if (debug::enabled(debug::TOPIC_CLOCK))
    debug::print("clock: %llu\n", clock);
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n", branchCorrect,
                 branchTotal,
                 branchTotal ? 100.0 * branchCorrect / branchTotal : 0.0);
  std::cout << std::dec << (REGModule.readReg(haltRd) & 0xFF) << std::endl;
}