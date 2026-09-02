#include "../include/StaticArbiter.hpp"
#include "../include/DCache.hpp"
#include "../include/Decoder.hpp"
#include "../include/PRF.hpp"
#include "../include/RAT.hpp"
#include "../include/ROB.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstdint>

CDBOutput CDBArbiter::build(const ALU &ALUModule, const LQ &LQModule,
                            const SquashInfo &squash) {
  CDBCandidate aluCand{};
  if (!ALUModule.isEmpty()) {
    aluCand.valid = true;
    aluCand.result.value = ALUModule.headValue();
    aluCand.result.robTag = ALUModule.headRobTag();
    aluCand.result.isControl = ALUModule.headIsControl();
  }
  CDBCandidate lsqCand{};
  auto lsqCDBDetect = LQModule.CDBDetect();
  if (lsqCDBDetect != -1) {
    lsqCand.valid = true;
    lsqCand.memIndex = static_cast<uint8_t>(lsqCDBDetect);
    lsqCand.result.robTag = LQModule.getRobTag(lsqCDBDetect);
    lsqCand.result.value = LQModule.getValue(lsqCDBDetect);
  }
  return arbitrate(aluCand, lsqCand, squash);
}

CDBBus CDBBus::build(const CDBOutput &cdbOut, const SquashInfo &squashDetect) {
  CDBBus cdbBus{};
  if (cdbOut.valid) {
    auto &r = cdbOut.result;
    bool guard = !squashDetect.needSquash ||
                 ROB::isOlder(r.robTag, squashDetect.SquashTag);
    cdbBus.lsqSetCDB = guard && cdbOut.memGranted;
    cdbBus.memIndex = cdbOut.memIndex;
  }
  return cdbBus;
}

CDBOutput CDBArbiter::arbitrate(const CDBCandidate &aluCandidate,
                                const CDBCandidate &lsqCandidate,
                                const SquashInfo &squash) {
  bool needSquash = squash.needSquash;
  uint8_t squashTag = squash.SquashTag;
  bool aluValid = aluCandidate.valid;
  ArithmeticCalculateResult aluResult = aluCandidate.result;
  if (aluValid && needSquash && !ROB::isOlder(aluResult.robTag, squashTag))
    aluValid = false;

  bool lsqValid = lsqCandidate.valid;
  ArithmeticCalculateResult lsqResult = lsqCandidate.result;
  if (lsqValid && needSquash && !ROB::isOlder(lsqResult.robTag, squashTag))
    lsqValid = false;
  CDBOutput out = {};

  if (!aluValid && !lsqValid)
    return out;

  if (aluValid && !lsqValid) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
    return out;
  }

  if (!aluValid && lsqValid) {
    out.result = lsqResult;
    out.valid = true;
    out.memGranted = true;
    out.memIndex = lsqCandidate.memIndex;
    return out;
  }

  if (ROB::isOlder(aluResult.robTag, lsqResult.robTag) ||
      aluResult.robTag == lsqResult.robTag) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
  } else {
    out.result = lsqResult;
    out.valid = true;
    out.memGranted = true;
    out.memIndex = lsqCandidate.memIndex;
  }
  return out;
}

DispatchBus DispatchArbiter::arbitrate(const RSUnit &RS, const ALU &ALU,
                                       const AGU &AGU, const BRU &BRU,
                                       const ROB &ROB, const PRF &PRF,
                                       const SquashInfo &squash) {
  DispatchBus dispatch;
  auto opReady = [&PRF](const Operand &op) { return PRF.isOperandReady(op); };
  if (!ALU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < INTEGERRS_CAP; ++i) {
      auto rs = RS.integerRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
        }
      }
    }
    if (foundAny) {
      dispatch.alu.rsIndex = best;
      dispatch.alu.robTag = bestTag;
      dispatch.alu.rsType = RSType::Integer;
      dispatch.alu.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.alu.valid = false;
    }
  }
  if (!AGU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    RSType bestType = RSType::Load;
    for (int i = 0; i < LOADRS_CAP; ++i) {
      auto rs = RS.loadRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
          bestType = RSType::Load;
        }
      }
    }
    for (int i = 0; i < STORERS_CAP; ++i) {
      auto rs = RS.storeAddressRS[i];
      if (!rs.free && opReady(rs.src1)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
          bestType = RSType::StoreAddr;
        }
      }
    }
    if (foundAny) {
      dispatch.agu.rsIndex = best;
      dispatch.agu.robTag = bestTag;
      dispatch.agu.rsType = bestType;
      dispatch.agu.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.agu.valid = false;
    }
  }
  if (!BRU.isFull()) {
    bool foundAny = false;
    int best = -1;
    uint8_t bestTag = 0;
    for (int i = 0; i < BRANCHRS_CAP; ++i) {
      auto rs = RS.branchRS[i];
      if (!rs.free && opReady(rs.src1) && opReady(rs.src2)) {
        auto tag = rs.robTag;
        if (!foundAny || ROB::isOlder(tag, bestTag)) {
          foundAny = true;
          best = i;
          bestTag = tag;
        }
      }
    }
    if (foundAny) {
      dispatch.bru.rsIndex = best;
      dispatch.bru.robTag = bestTag;
      dispatch.bru.rsType = RSType::Branch;
      dispatch.bru.valid = true;
      if (squash.needSquash && !ROB::isOlder(bestTag, squash.SquashTag))
        dispatch.bru.valid = false;
    }
  }
  return dispatch;
}

MemDispatchDecision MemArbiter::arbitrate(const LQ &LQ, const SQ &SQ,
                                                 const ROB &rob,
                                                 const DCache &dcache,
                                                 const SquashInfo &squash) {
  MemDispatchDecision memDecision{};
  if (!dcache.isBusy() && !SQ.isEmpty()) {
    auto storeTag = SQ.headRobTag();
    bool committed = rob.isEmpty() || ROB::isOlder(storeTag, rob.getHead());
    bool atHeadReady = !committed && SQ.headRobTag() == rob.getHead() &&
                       rob.isCommitReadyAt(((SQ.headRobTag() & 0x3F)));
    if (committed || atHeadReady) {
      MemRequest newRequest{};
      newRequest.address = SQ.getAddress(SQ.getHead());
      newRequest.value = SQ.getValue(SQ.getHead());
      newRequest.n_bytes = SQ.getNBytes(SQ.getHead());
      newRequest.op = Operation::Store;
      newRequest.robTag = storeTag;
      newRequest.memIndex = static_cast<uint8_t>(SQ.getHead() | MEM_STORE_BIT);
      memDecision.valid = true;
      memDecision.request = newRequest;
    }
  }
  if (!memDecision.valid) {
    auto loadIndex = LQ.LoadDetect();
    if (loadIndex != 0xFFFFFFFF && !dcache.isBusy()) {
      auto loadAddr = LQ.getAddress(loadIndex);
      auto loadTag = LQ.getRobTag(loadIndex);
      if (SQ.canDispatchLoad(loadAddr, loadTag)) {
        MemRequest newRequest{};
        newRequest.address = LQ.getAddress(loadIndex);
        newRequest.isSigned = !LQ.getIsUnsigned(loadIndex);
        newRequest.n_bytes = LQ.getNBytes(loadIndex);
        newRequest.op = Operation::Load;
        newRequest.robTag = LQ.getRobTag(loadIndex);
        newRequest.memIndex = static_cast<uint8_t>(loadIndex);
        if (!squash.needSquash ||
            (squash.needSquash &&
             ROB::isOlder(newRequest.robTag, squash.SquashTag))) {
          memDecision.valid = true;
          memDecision.request = newRequest;
        }
      }
    }
  }
  return memDecision;
}

// Resolve an architectural register source into an Operand. RS no longer
// caches values: register sources store only the phy tag (readiness/read from
// PRF at dispatch), x0 is a constant zero, and immediates/PC are constants.
static Operand resolveSrc(const IssueArbiterInput &input, int regNum) {
  auto op = input.RATModule.readOperand(regNum);
  if (op.ready)  // x0: constant zero
    return {.tag = InvalidPhy, .imm = op.value};
  assert(op.phyRegIndex != InvalidPhy); // P0 is never mapped (P0-dead invariant)
  return {.tag = op.phyRegIndex, .imm = 0};
}

IssuePacket IssueArbiter::issue_IntegerRS(const IssueArbiterInput &input,
                                          const UopView &inst, bool has_rs2,
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
  p.robTag = input.ROBModule.getNextTag();
  p.integerRS.free = false;
  p.integerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  p.integerRS.src1 = resolveSrc(input, inst.rs1);
  if (imm_as_vk) {
    p.integerRS.src2 = {.tag = InvalidPhy, .imm = inst.imm};
  } else if (has_rs2) {
    p.integerRS.src2 = resolveSrc(input, inst.rs2);
  }
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lqtTailSnapshot = input.LQModule.getTail();
  p.robEntry.sqTailSnapshot = input.SQModule.getTail();
  p.robEntry.ckptId = inst.ckptId;
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : InvalidPhy;
  p.robEntry.newPhy = p.phy;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  if (isControl) {
    p.isControl = true;
    p.pc = inst.pc;
    p.robEntry.type = ROBType::LINK;
    p.robEntry.isIndirect = true; // JALR path: target is register-driven
    if (inst.rd == 0 && inst.rs1 == 1 && inst.imm == 0)
      p.robEntry.isRet = true;  // JALR x0, 0(x1): return
  }
  p.integerRS.robTag = p.robTag;
  return p;
}

IssuePacket IssueArbiter::issue_UandJ(const IssueArbiterInput &input,
                                      const UopView &inst, bool has_PC,
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
  p.robTag = input.ROBModule.getNextTag();
  p.integerRS.free = false;
  p.integerRS.op = decodeOp(inst);
  auto destination = inst.rd;
  if (has_PC) {
    p.integerRS.src1 = {.tag = InvalidPhy, .imm = static_cast<int32_t>(inst.pc)};
  }
  p.integerRS.src2 = {.tag = InvalidPhy, .imm = inst.imm};
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lqtTailSnapshot = input.LQModule.getTail();
  p.robEntry.sqTailSnapshot = input.SQModule.getTail();
  p.robEntry.ckptId = inst.ckptId;
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : InvalidPhy;
  p.robEntry.newPhy = p.phy;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  if (isControl) {
    p.isControl = true;
    p.pc = inst.pc;
    p.robEntry.type = ROBType::LINK;
    if (inst.rd == 1)
      p.robEntry.isCall = true;  // JAL with return address register
  }
  p.integerRS.robTag = p.robTag;
  return p;
}

IssuePacket IssueArbiter::issue_B(const IssueArbiterInput &input,
                                  const UopView &inst) {
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
  p.robTag = input.ROBModule.getNextTag();
  p.branchRS.free = false;
  p.branchRS.op = decodeOp(inst);
  p.branchRS.imm = inst.imm;
  p.branchRS.pc = inst.pc;
  p.branchRS.src1 = resolveSrc(input, inst.rs1);
  p.branchRS.src2 = resolveSrc(input, inst.rs2);
  p.robEntry = ROBEntry(ROBType::BRANCH);
  p.robEntry.pc = inst.pc;
  p.robEntry.predictedPC = inst.predictedPC;
  p.robEntry.lqtTailSnapshot = input.LQModule.getTail();
  p.robEntry.sqTailSnapshot = input.SQModule.getTail();
  p.robEntry.ckptId = inst.ckptId;
  p.branchRS.robTag = p.robTag;
  return p;
}

IssuePacket IssueArbiter::issue_Load(const IssueArbiterInput &input,
                                     const UopView &inst, int n_bytes,
                                     bool isUnsigned) {
  IssuePacket p{};
  if (input.ROBModule.isFull() || input.LQModule.isFull()) {
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
  p.robTag = input.ROBModule.getNextTag();
  p.loadRS.memIndex = input.LQModule.getTail();
  p.loadRS.free = false;
  p.loadRS.op = decodeOp(inst);
  auto destination = inst.rd;

  p.loadRS.src1 = resolveSrc(input, inst.rs1);
  p.loadRS.src2 = {.tag = InvalidPhy, .imm = inst.imm};
  if (inst.allocDest && !input.PRFModule.isFreeListEmpty()) {
    p.allocDest = true;
    p.phy = input.PRFModule.getFreeListSlot(input.PRFModule.getHeadSeq());
  }
  p.robEntry = ROBEntry(ROBType::REGISTER);
  p.robEntry.dest = destination;
  p.robEntry.pc = inst.pc;
  // Include-self boundary (see LQ::getTailSnapshot): an exclusive snapshot
  // let a LoadViolation squash -- whose target is the load itself -- rewind
  // the LQ over its own entry, freezing retirement at that row.
  p.robEntry.lqtTailSnapshot = input.LQModule.getTailSnapshot();
  p.robEntry.sqTailSnapshot = input.SQModule.getTail();
  p.robEntry.oldPhy =
      inst.allocDest ? input.RATModule.readRAT_PRF(destination) : InvalidPhy;
  p.robEntry.newPhy = p.phy;
  p.robEntry.ckptId = inst.ckptId;
  if (debug::enabled(debug::TOPIC_PRF) && inst.allocDest)
    debug::print("PRF rename x%d <- P%d (old=P%d)\n", destination, p.phy,
                 p.robEntry.oldPhy);
  p.loadRS.robTag = p.robTag;
  return p;
}

IssuePacket IssueArbiter::issue_Store(const IssueArbiterInput &input,
                                      const UopView &inst, int n_bytes) {
  IssuePacket p{};
  if (input.ROBModule.isFull() || input.SQModule.isFull()) {
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
  p.robTag = input.ROBModule.getNextTag();
  p.storeAddrRS.memIndex =
      static_cast<uint8_t>(input.SQModule.getTail() | MEM_STORE_BIT);
  p.storeValueRS.memIndex = p.storeAddrRS.memIndex;
  p.storeAddrRS.free = false;
  p.storeAddrRS.op = decodeOp(inst);

  p.storeAddrRS.src1 = resolveSrc(input, inst.rs1);
  p.storeAddrRS.src2 = {.tag = InvalidPhy, .imm = inst.imm};

  p.storeValueRS.free = false;
  p.storeValueRS.data = resolveSrc(input, inst.rs2);
  p.robEntry = ROBEntry(ROBType::STORE);
  p.robEntry.pc = inst.pc;
  // Include-self boundary on the SQ side; the store never enters the LQ,
  // so its lqtTailSnapshot stays the raw tail.
  p.robEntry.lqtTailSnapshot = input.LQModule.getTail();
  p.robEntry.sqTailSnapshot = input.SQModule.getTailSnapshot();
  p.robEntry.ckptId = inst.ckptId;
  p.storeAddrRS.robTag = p.robTag;
  p.storeValueRS.robTag = p.robTag;
  return p;
}

Operation IssueArbiter::decodeOp(const UopView &inst) {
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
  UopView inst;
  inst.type = input.DecodeUnitModule.headType();
  inst.opcode = input.DecodeUnitModule.headOpcode();
  inst.funct3 = input.DecodeUnitModule.headFunct3();
  inst.funct7 = input.DecodeUnitModule.headFunct7();
  inst.rd = input.DecodeUnitModule.headRd();
  inst.rs1 = input.DecodeUnitModule.headRs1();
  inst.rs2 = input.DecodeUnitModule.headRs2();
  inst.imm = input.DecodeUnitModule.headImm();
  inst.pc = input.DecodeUnitModule.headPc();
  inst.isHalt = input.DecodeUnitModule.headIsHalt();
  inst.allocDest = input.DecodeUnitModule.headAllocDest();
  inst.predictedPC = input.DecodeUnitModule.headPredictedPC();
  inst.ckptId = input.DecodeUnitModule.headCkptId();
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
        issuePacket.robTag = input.ROBModule.getNextTag();
        issuePacket.robEntry = ROBEntry(ROBType::REGISTER);
        issuePacket.robEntry.dest = inst.rd;
        issuePacket.robEntry.ckptId = inst.ckptId;
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
