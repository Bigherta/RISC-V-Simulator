#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

CPU::CPU(Memory mem) : CPUstate(mem), InstructMem(mem), DMEMModule(mem) {}

void CPU::fetch() {
  if (squashDetect.needSquash) {
    CPUstate.FQModule.clear();
    CPUstate.programCounter = squashDetect.SquashPC;
    CPUstate.haltFetched = false;
    return;
  }
  if (haltFetched)
    return;
  const auto raw_inst = InstructMem.read_inst(programCounter);
  if (raw_inst == 0x0ff00513)
    CPUstate.haltFetched = true;
  if (!FQModule.isFull()) {
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
    CPUstate.FQModule.push(raw_inst, programCounter, predictedPC, ckpt);
    CPUstate.programCounter = predictedPC;
  }
}

int CPU::issue_IntegerRS(const Uop &inst, bool has_rs2, bool imm_as_vk,
                         bool isControl) {
  if (ROBModule.isFull()) {
    return -1;
  }
  int integerSlot = RSModule.tryAllocInteger();
  if (integerSlot < 0) {
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
  newROB.predictedPC = inst.predictedPC;
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.oldPhy = inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  if (isControl) {
    newROB.type = ROBType::LINK;
    newROB.ckpt.BPsnapshot = inst.BPSnapshot;
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
  CPUstate.RSModule.integerRS[integerSlot] = IntegerRS;
  return robIndex;
}

int CPU::issue_UandJ(const Uop &inst, bool has_PC, bool isControl) {
  if (ROBModule.isFull()) {
    return -1;
  }
  int integerSlot = RSModule.tryAllocInteger();
  if (integerSlot < 0) {
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
  newROB.predictedPC = inst.predictedPC;
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.oldPhy = inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  if (isControl) {
    newROB.type = ROBType::LINK;
    newROB.ckpt.BPsnapshot = inst.BPSnapshot;
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
  CPUstate.RSModule.integerRS[integerSlot] = IntegerRS;
  return robIndex;
}

int CPU::issue_B(const Uop &inst) {
  if (ROBModule.isFull()) {
    return -1;
  }
  int branchSlot = RSModule.tryAllocBranch();
  if (branchSlot < 0) {
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
  newROB.predictedPC = inst.predictedPC;
  newROB.lsqTailSnapshot = LSQModule.getTail();
  newROB.ckpt.BPsnapshot = inst.BPSnapshot;
  auto prfSnap = REGModule.snapshotRAT_PRF();
  memcpy(newROB.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
         sizeof(newROB.ckpt.RATsnapshot.RAT_snapshot));
  newROB.ckpt.flHeadSeqCkpt = PRFModule.getHeadSeq();
  int robIndex = CPUstate.ROBModule.push(newROB);
  BranchRS.robIndex = robIndex;
  CPUstate.RSModule.branchRS[branchSlot] = BranchRS;
  return robIndex;
}

int CPU::issue_Load(const Uop &inst, int n_bytes, bool isUnsigned) {
  if (ROBModule.isFull() || LSQModule.isFull()) {
    return -1;
  }
  int loadSlot = RSModule.tryAllocLoad();
  if (loadSlot < 0) {
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
  newROB.oldPhy = inst.allocDest ? REGModule.readRAT_PRF(destination) : -1;
  newROB.newPhy = Physical_regNum;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination,
                 Physical_regNum, newROB.oldPhy);
  int robIndex = CPUstate.ROBModule.push(newROB);
  LoadRS.robIndex = robIndex;
  CPUstate.LSQModule.pushLoad(robIndex, CPUstate.ROBModule.getSeq(robIndex),
                              n_bytes, isUnsigned);
  CPUstate.RSModule.loadRS[loadSlot] = LoadRS;
  return robIndex;
}

int CPU::issue_Store(const Uop &inst, int n_bytes) {

  if (ROBModule.isFull() || LSQModule.isFull()) {
    return -1;
  }
  int storeAddrSlot = RSModule.tryAllocStoreAddress();
  int storeValueSlot = RSModule.tryAllocStoreValue();
  if (storeAddrSlot < 0 || storeValueSlot < 0) {
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
  CPUstate.RSModule.storeAddressRS[storeAddrSlot] = StoreRS;
  CPUstate.RSModule.storeValueRS[storeValueSlot] = MicroRS;
  return robIndex;
}

void CPU::issue() {
  if (!squashDetect.needSquash) {
    int res = -1;
    if (!DecodeUnitModule.isEmpty()) {
      const auto &inst = DecodeUnitModule.headUop();
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
        CPUstate.DecodeUnitModule.pop();
        res = -1;
        break;
      }
      }
    }
    if (res != -1)
      CPUstate.DecodeUnitModule.pop();
  }
}

Operation CPU::decodeOp(const Uop &inst) {
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

CDBBypassResult CPU::CDBBypass(int phy) const {
  CDBBypassResult out;
  if (cdbArbiter.valid && !cdbArbiter.result.isControl &&
      ROBModule.getNewPhy(cdbArbiter.result.robIndex) == phy) {
    out.valid = true;
    out.value = cdbArbiter.result.value;
  }
  return out;
}

void CPU::commit() {
  if (squashDetect.needSquash) {
    CPUstate.ROBModule.flush(squashDetect.SquashIndex);
    return;
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
    // 3. clear the wrong RAT_PRF
    // only modified by the issue instructions after the control inst
    if (squashDetect.SquashIndex >= 0) {
      RATSnapshot snapP;
      memcpy(snapP.RAT_snapshot,
             ROBModule.getRATPrfCkpt(squashDetect.SquashIndex),
             sizeof(snapP.RAT_snapshot));
      CPUstate.REGModule.restoreRAT_PRF(snapP);
    }
    // 4. clear the old flushArbiter elements
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
  memcpy(&RSModule, &CPUstate.RSModule, sizeof(RSModule));
  memcpy(&REGModule, &CPUstate.REGModule, sizeof(REGModule));
  memcpy(&ROBModule, &CPUstate.ROBModule, sizeof(ROBModule));
  memcpy(&ALUModule, &CPUstate.ALUModule, sizeof(ALUModule));
  memcpy(&AGUModule, &CPUstate.AGUModule, sizeof(AGUModule));
  memcpy(&BRUModule, &CPUstate.BRUModule, sizeof(BRUModule));
  memcpy(&LSQModule, &CPUstate.LSQModule, sizeof(LSQModule));
  memcpy(&FQModule, &CPUstate.FQModule, sizeof(FQModule));
  memcpy(&DecodeUnitModule, &CPUstate.DecodeUnitModule,
         sizeof(DecodeUnitModule));
  memcpy(&PRFModule, &CPUstate.PRFModule, sizeof(PRFModule));
  memcpy(&BPModule, &CPUstate.BPModule, sizeof(BPModule));
  memcpy(&flushArbiter, &CPUstate.flushArbiter, sizeof(flushArbiter));
  DMEMModule.snapshotFrom(CPUstate.DMEMModule);
  programCounter = CPUstate.programCounter;
  haltFetched = CPUstate.haltFetched;
  haltCommitted = CPUstate.haltCommitted;
  haltRd = CPUstate.haltRd;
  squashDetect = CPUstate.flushArbiter.arbitResult();
  CDBCandidate aluCand{};
  if (!ALUModule.isEmpty()) {
    aluCand.valid = true;
    aluCand.result.value = ALUModule.headValue();
    aluCand.result.robIndex = ALUModule.headRobIndex();
    aluCand.result.robSeq = ALUModule.headRobSeq();
    aluCand.result.isControl = ALUModule.headIsControl();
  }
  CDBCandidate lsqCand{};
  auto lsqCDBDetect = LSQModule.CDBDetect();
  if (lsqCDBDetect != -1) {
    lsqCand.valid = true;
    lsqCand.result.robIndex = LSQModule.getRobIndex(lsqCDBDetect);
    lsqCand.result.robSeq = LSQModule.getRobSeq(lsqCDBDetect);
    lsqCand.result.value = LSQModule.getValue(lsqCDBDetect);
  }
  cdbArbiter = CDBArbiter::arbitrate(aluCand, lsqCand, squashDetect);
  DispatchBus dispatchBus = DispatchArbiter::arbitrate(
      RSModule, ALUModule, AGUModule, BRUModule, ROBModule, squashDetect);
  aguInput.squashDetect = squashDetect;
  aluInput.squashDetect = squashDetect;
  aluInput.cdbArbiter = cdbArbiter;
  aluInput.dispatch = dispatchBus.alu;
  aguInput.dispatch = dispatchBus.agu;
  bruInput.dispatch = dispatchBus.bru;
  rsInput.dispatchBus = dispatchBus;
  dmemInput.squashDetect = squashDetect;
  cdbInput.squashDetect = squashDetect;
  cdbInput.cdbArbiter = cdbArbiter;
  decodeInput.squashDetect = squashDetect;
  lsqInput.squashDetect = squashDetect;
  rsInput.squashDetect = squashDetect;
  robInput.squashDetect = squashDetect;
  robInput.cdbArbiter = cdbArbiter;
  prfInput.squashDetect = squashDetect;
  prfInput.cdbArbiter = cdbArbiter;
  bruInput.squashDetect = squashDetect;
  bpInput.squashDetect = squashDetect;
  bpInput.cdbArbiter = cdbArbiter;
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
    LSQModule.tick(lsqInput, CPUstate);
    ROBModule.tick(robInput, CPUstate);
    PRFModule.tick(prfInput, CPUstate);
    CDBModule.tick(cdbInput, CPUstate);
    ALUModule.tick(aluInput, CPUstate);
    AGUModule.tick(aguInput, CPUstate);  
    BRUModule.tick(bruInput, CPUstate);  
    BPModule.tick(bpInput, CPUstate); 
    DMEMModule.tick(dmemInput, CPUstate);
    commit();
    RSModule.tick(rsInput, CPUstate);
    flush();
    DecodeUnitModule.tick(decodeInput, CPUstate);
    CPUstate.REGModule.resetX0();
    assert(checkPRFInvariant());
    ++clock;
    finish = haltCommitted && FQModule.isEmpty() && DecodeUnitModule.isEmpty() &&
             ROBModule.isEmpty();
  }
  if (debug::enabled(debug::TOPIC_CLOCK))
    debug::print("clock: %llu\n", clock);
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n", CPUstate.branchCorrect,
                 CPUstate.branchTotal,
                 CPUstate.branchTotal
                     ? 100.0 * CPUstate.branchCorrect / CPUstate.branchTotal
                     : 0.0);
  std::cout << std::dec << (REGModule.readReg(haltRd) & 0xFF) << std::endl;
}