#include "../include/IssueArbiter.hpp"
#include "../include/util.hpp"
#include "../include/Decoder.hpp"
#include "../include/PRF.hpp"
#include "../include/RAT.hpp"
#include "../include/ROB.hpp"
CDBBypassResult IssueArbiter::CDBBypass(const IssueArbiterInput &input,
                                        int phy) {
  CDBBypassResult out{};
  if (input.cdbout.valid && !input.cdbout.result.isControl &&
      input.ROBModule.getNewPhy(input.cdbout.result.robIndex) == phy) {
    out.valid = true;
    out.value = input.cdbout.result.value;
  }
  return out;
}

IssuePacket IssueArbiter::issue_IntegerRS(const IssueArbiterInput &input,
                                          const Uop &inst, bool has_rs2,
                                          bool imm_as_vk, bool isControl) {
  IssuePacket p{};
  if (input.ROBModule.isFull()) {
    return p;
  }
  int integerSlot = input.RSModule.tryAllocInteger();
  if (integerSlot < 0) {
    return p;
  }
  p.valid = true;
  p.hasInteger = true;
  p.integerSlot = integerSlot;
  p.robIndex = input.ROBModule.getTail();
  p.robSeq = input.ROBModule.getNextSeq();
  p.integerRS.free = false;
  p.integerRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto destination = inst.rd;
  auto op1 = input.RATModule.readOperand(regNum1);
  if (op1.ready) {
    p.integerRS.vj = op1.value;
  } else {
    if (input.PRFModule.isReady(op1.phyRegIndex)) {
      p.integerRS.vj = input.PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op1.phyRegIndex);
      if (bypass.valid) {
        p.integerRS.vj = bypass.value;
      } else {
        p.integerRS.qj = op1.phyRegIndex;
      }
    }
  }
  if (imm_as_vk) {
    p.integerRS.vk = inst.imm;
  } else if (has_rs2) {
    auto op2 = input.RATModule.readOperand(regNum2);
    if (op2.ready) {
      p.integerRS.vk = op2.value;
    } else {
      if (input.PRFModule.isReady(op2.phyRegIndex)) {
        p.integerRS.vk = input.PRFModule.getValue(op2.phyRegIndex);
      } else {
        auto bypass = CDBBypass(input, op2.phyRegIndex);
        if (bypass.valid) {
          p.integerRS.vk = bypass.value;
        } else {
          p.integerRS.qk = op2.phyRegIndex;
        }
      }
    }
  }
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lsqTailSnapshot = input.LSQModule.getTail();
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : -1;
  p.robEntry.newPhy = p.phy;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  if (isControl) {
    p.isControl = true;
    p.pc = inst.pc;
    p.robEntry.type = ROBType::LINK;
    p.robEntry.ckpt.BPsnapshot = inst.BPSnapshot;
    auto prfSnap = input.RATModule.snapshotRAT_PRF();
    memcpy(p.robEntry.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
           sizeof(p.robEntry.ckpt.RATsnapshot.RAT_snapshot));
    if (inst.allocDest)
      p.robEntry.ckpt.RATsnapshot.RAT_snapshot[destination] = p.phy;
    p.robEntry.ckpt.flHeadSeqCkpt =
        input.PRFModule.getHeadSeq() + (inst.allocDest ? 1 : 0);
  }
  p.integerRS.robIndex = p.robIndex;
  return p;
}

IssuePacket IssueArbiter::issue_UandJ(const IssueArbiterInput &input,
                                      const Uop &inst, bool has_PC,
                                      bool isControl) {
  IssuePacket p{};
  if (input.ROBModule.isFull()) {
    return p;
  }
  int integerSlot = input.RSModule.tryAllocInteger();
  if (integerSlot < 0) {
    return p;
  }
  p.valid = true;
  p.hasInteger = true;
  p.integerSlot = integerSlot;
  p.robIndex = input.ROBModule.getTail();
  p.robSeq = input.ROBModule.getNextSeq();
  p.integerRS.free = false;
  p.integerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  if (has_PC) {
    p.integerRS.vj = inst.pc;
  }
  p.integerRS.vk = inst.imm;
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lsqTailSnapshot = input.LSQModule.getTail();
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : -1;
  p.robEntry.newPhy = p.phy;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  if (isControl) {
    p.isControl = true;
    p.pc = inst.pc;
    p.robEntry.type = ROBType::LINK;
    p.robEntry.ckpt.BPsnapshot = inst.BPSnapshot;
    auto prfSnap = input.RATModule.snapshotRAT_PRF();
    memcpy(p.robEntry.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
           sizeof(p.robEntry.ckpt.RATsnapshot.RAT_snapshot));
    if (inst.allocDest)
      p.robEntry.ckpt.RATsnapshot.RAT_snapshot[destination] = p.phy;
    p.robEntry.ckpt.flHeadSeqCkpt =
        input.PRFModule.getHeadSeq() + (inst.allocDest ? 1 : 0);
  }
  p.integerRS.robIndex = p.robIndex;
  return p;
}

IssuePacket IssueArbiter::issue_B(const IssueArbiterInput &input,
                                  const Uop &inst) {
  IssuePacket p{};
  if (input.ROBModule.isFull()) {
    return p;
  }
  int branchSlot = input.RSModule.tryAllocBranch();
  if (branchSlot < 0) {
    return p;
  }
  p.valid = true;
  p.hasBranch = true;
  p.branchSlot = branchSlot;
  p.robIndex = input.ROBModule.getTail();
  p.robSeq = input.ROBModule.getNextSeq();
  p.branchRS.free = false;
  p.branchRS.op = decodeOp(inst);
  p.branchRS.imm = inst.imm;
  p.branchRS.pc = inst.pc;
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;
  auto op1 = input.RATModule.readOperand(regNum1);
  if (op1.ready) {
    p.branchRS.vj = op1.value;
  } else {
    if (input.PRFModule.isReady(op1.phyRegIndex)) {
      p.branchRS.vj = input.PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op1.phyRegIndex);
      if (bypass.valid) {
        p.branchRS.vj = bypass.value;
      } else {
        p.branchRS.qj = op1.phyRegIndex;
      }
    }
  }
  auto op2 = input.RATModule.readOperand(regNum2);
  if (op2.ready) {
    p.branchRS.vk = op2.value;
  } else {
    if (input.PRFModule.isReady(op2.phyRegIndex)) {
      p.branchRS.vk = input.PRFModule.getValue(op2.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op2.phyRegIndex);
      if (bypass.valid) {
        p.branchRS.vk = bypass.value;
      } else {
        p.branchRS.qk = op2.phyRegIndex;
      }
    }
  }
  p.robEntry = ROBEntry(ROBType::BRANCH);
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lsqTailSnapshot = input.LSQModule.getTail();
  p.robEntry.ckpt.BPsnapshot = inst.BPSnapshot;
  auto prfSnap = input.RATModule.snapshotRAT_PRF();
  memcpy(p.robEntry.ckpt.RATsnapshot.RAT_snapshot, prfSnap.RAT_snapshot,
         sizeof(p.robEntry.ckpt.RATsnapshot.RAT_snapshot));
  p.robEntry.ckpt.flHeadSeqCkpt = input.PRFModule.getHeadSeq();
  p.branchRS.robIndex = p.robIndex;
  return p;
}

IssuePacket IssueArbiter::issue_Load(const IssueArbiterInput &input,
                                     const Uop &inst, int n_bytes,
                                     bool isUnsigned) {
  IssuePacket p{};
  if (input.ROBModule.isFull() || input.LSQModule.isFull()) {
    return p;
  }
  int loadSlot = input.RSModule.tryAllocLoad();
  if (loadSlot < 0) {
    return p;
  }
  p.valid = true;
  p.hasLoad = true;
  p.isLoad = true;
  p.loadSlot = loadSlot;
  p.nBytes = n_bytes;
  p.isUnsigned = isUnsigned;
  p.robIndex = input.ROBModule.getTail();
  p.robSeq = input.ROBModule.getNextSeq();
  p.loadRS.free = false;
  p.loadRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto destination = inst.rd;

  auto op1 = input.RATModule.readOperand(regNum1);
  if (op1.ready) {
    p.loadRS.vj = op1.value;
  } else {
    if (input.PRFModule.isReady(op1.phyRegIndex)) {
      p.loadRS.vj = input.PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op1.phyRegIndex);
      if (bypass.valid) {
        p.loadRS.vj = bypass.value;
      } else {
        p.loadRS.qj = op1.phyRegIndex;
      }
    }
  }
  p.loadRS.vk = inst.imm;
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : -1;
  p.robEntry.newPhy = p.phy;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  p.loadRS.robIndex = p.robIndex;
  return p;
}

IssuePacket IssueArbiter::issue_Store(const IssueArbiterInput &input,
                                      const Uop &inst, int n_bytes) {
  IssuePacket p{};
  if (input.ROBModule.isFull() || input.LSQModule.isFull()) {
    return p;
  }
  int storeAddrSlot = input.RSModule.tryAllocStoreAddress();
  int storeValueSlot = input.RSModule.tryAllocStoreValue();
  if (storeAddrSlot < 0 || storeValueSlot < 0) {
    return p;
  }
  p.valid = true;
  p.hasStore = true;
  p.isStore = true;
  p.storeAddrSlot = storeAddrSlot;
  p.storeValueSlot = storeValueSlot;
  p.nBytes = n_bytes;
  p.robIndex = input.ROBModule.getTail();
  p.robSeq = input.ROBModule.getNextSeq();
  p.storeAddrRS.free = false;
  p.storeAddrRS.op = decodeOp(inst);
  auto regNum1 = inst.rs1;
  auto regNum2 = inst.rs2;

  auto op1 = input.RATModule.readOperand(regNum1);
  if (op1.ready) {
    p.storeAddrRS.vj = op1.value;
  } else {
    if (input.PRFModule.isReady(op1.phyRegIndex)) {
      p.storeAddrRS.vj = input.PRFModule.getValue(op1.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op1.phyRegIndex);
      if (bypass.valid) {
        p.storeAddrRS.vj = bypass.value;
      } else {
        p.storeAddrRS.qj = op1.phyRegIndex;
      }
    }
  }
  p.storeAddrRS.vk = inst.imm;

  p.storeValueRS.free = false;
  auto op2 = input.RATModule.readOperand(regNum2);
  if (op2.ready) {
    p.storeValueRS.vrs2 = op2.value;
  } else {
    if (input.PRFModule.isReady(op2.phyRegIndex)) {
      p.storeValueRS.vrs2 = input.PRFModule.getValue(op2.phyRegIndex);
    } else {
      auto bypass = CDBBypass(input, op2.phyRegIndex);
      if (bypass.valid) {
        p.storeValueRS.vrs2 = bypass.value;
      } else {
        p.storeValueRS.qrs2 = op2.phyRegIndex;
      }
    }
  }
  p.robEntry = ROBEntry(ROBType::STORE);
  p.storeAddrRS.robIndex = p.robIndex;
  p.storeValueRS.robIndex = p.robIndex;
  return p;
}

Operation IssueArbiter::decodeOp(const Uop &inst) {
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

IssuePacket IssueArbiter::build(const IssueArbiterInput &input) {
  IssuePacket issuePacket{};
  if (input.squashDetect.needSquash || input.DecodeUnitModule.isEmpty())
    return issuePacket;
  const auto &inst = input.DecodeUnitModule.headUop();
  switch (inst.type) {
  case RISC_V::R: {
    issuePacket = issue_IntegerRS(input, inst, true, false, false);
    break;
  }
  case RISC_V::I: {
    if (inst.opcode == 0b0010011) {
      if (inst.isHalt) {
        issuePacket.valid = true;
        issuePacket.isHalt = true;
        issuePacket.robIndex = input.ROBModule.getTail();
        issuePacket.robSeq = input.ROBModule.getNextSeq();
        issuePacket.robEntry = ROBEntry(ROBType::REGISTER);
        issuePacket.robEntry.dest = inst.rd;
        issuePacket.robEntry.halt = true;
        issuePacket.robEntry.isCommitReady = true;
      } else {
        issuePacket = issue_IntegerRS(input, inst, false, true, false);
      }
    } else if (inst.opcode == 0b1100111) {
      issuePacket = issue_IntegerRS(input, inst, false, true, true);
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
      issuePacket = issue_Load(input, inst, n_bytes, isUnsigned);
    }
    break;
  }
  case RISC_V::Istar: {
    issuePacket = issue_IntegerRS(input, inst, false, true, false);
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
    issuePacket = issue_Store(input, inst, n_bytes);
    break;
  }
  case RISC_V::B: {
    issuePacket = issue_B(input, inst);
    break;
  }
  case RISC_V::U: {
    if (inst.opcode == 0b0010111) {
      issuePacket = issue_UandJ(input, inst, true, false);
    } else if (inst.opcode == 0b0110111) {
      issuePacket = issue_UandJ(input, inst, false, false);
    }
    break;
  }
  case RISC_V::J: {
    issuePacket = issue_UandJ(input, inst, true, true);
    break;
  }
  case RISC_V::RV_INVALID: {
    issuePacket.valid = true;
    break;
  }
  }
  return issuePacket;
}
