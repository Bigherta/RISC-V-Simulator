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


void CPU::read() {
  memcpy(&RSModule, &CPUstate.RSModule, sizeof(RSModule));
  memcpy(&RATModule, &CPUstate.RATModule, sizeof(RATModule));
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
  decodeInput.squashDetect = squashDetect;
  lsqInput.squashDetect = squashDetect;
  rsInput.squashDetect = squashDetect;
  robInput.squashDetect = squashDetect;
  robInput.cdbArbiter = cdbArbiter;
  prfInput.squashDetect = squashDetect;
  prfInput.cdbArbiter = cdbArbiter;
  ratInput.squashDetect = squashDetect;
  flarbInput.squashDetect = squashDetect;
  flarbInput.cdbArbiter = cdbArbiter;
  isarbInput.squashDetect = squashDetect;
  isarbInput.cdbout = cdbArbiter;
  issuePacket = IssueArbiter::build(isarbInput);
  bruInput.squashDetect = squashDetect;
  bpInput.squashDetect = squashDetect;
  bpInput.cdbArbiter = cdbArbiter;
  CDBBus cdbBus{};
  if (cdbArbiter.valid) {
    auto &r = cdbArbiter.result;
    bool guard = !squashDetect.needSquash || r.robSeq < squashDetect.SquashSeq;
    bool robOk = !ROBModule.isEmpty() && r.robSeq >= ROBModule.headSeq();
    cdbBus.broadcastValid = guard && (!r.isControl || robOk);
    cdbBus.broadcastValue =
        r.isControl ? PRFModule.getValue(ROBModule.getNewPhy(r.robIndex))
                    : r.value;
    cdbBus.lsqSetCDB = guard && cdbArbiter.lsqGranted;
    cdbBus.robIndex = r.robIndex;
    cdbBus.robSeq = r.robSeq;
  }
  lsqInput.cdbBus = cdbBus;
  rsInput.cdbBus = cdbBus;
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
    mark(CPUstate.RATModule.readRAT_PRF(r));
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
    LSQModule.tick(lsqInput, CPUstate);
    ROBModule.tick(robInput, CPUstate);
    PRFModule.tick(prfInput, CPUstate);
    ALUModule.tick(aluInput, CPUstate);
    AGUModule.tick(aguInput, CPUstate);
    BRUModule.tick(bruInput, CPUstate);
    BPModule.tick(bpInput, CPUstate);
    DMEMModule.tick(dmemInput, CPUstate);
    RSModule.tick(rsInput, CPUstate);
    RATModule.tick(ratInput, CPUstate);
    flushArbiter.tick(flarbInput, CPUstate);
    DecodeUnitModule.tick(decodeInput, CPUstate);
    assert(checkPRFInvariant());
    ++clock;
    finish = haltCommitted && FQModule.isEmpty() &&
             DecodeUnitModule.isEmpty() && ROBModule.isEmpty();
  }
  if (debug::enabled(debug::TOPIC_CLOCK))
    debug::print("clock: %llu\n", clock);
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n", CPUstate.branchCorrect,
                 CPUstate.branchTotal,
                 CPUstate.branchTotal
                     ? 100.0 * CPUstate.branchCorrect / CPUstate.branchTotal
                     : 0.0);
  std::cout << std::dec
            << (PRFModule.getValue(RATModule.readRAT_PRF(haltRd)) & 0xFF)
            << std::endl;
}