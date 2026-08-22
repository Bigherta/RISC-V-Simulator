#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

CPU::CPU(Memory mem)
    : CPUstate(mem), InstructMem(mem), IMEMModule(mem), DMEMModule(mem) {}

void CPU::comb() {
  memcpy(&RSModule, &CPUstate.RSModule, sizeof(RSModule));
  memcpy(&RATModule, &CPUstate.RATModule, sizeof(RATModule));
  memcpy(&ROBModule, &CPUstate.ROBModule, sizeof(ROBModule));
  memcpy(&ALUModule, &CPUstate.ALUModule, sizeof(ALUModule));
  memcpy(&AGUModule, &CPUstate.AGUModule, sizeof(AGUModule));
  memcpy(&BRUModule, &CPUstate.BRUModule, sizeof(BRUModule));
  memcpy(&LQModule, &CPUstate.LQModule, sizeof(LQModule));
  memcpy(&SQModule, &CPUstate.SQModule, sizeof(SQModule));
  memcpy(&FQModule, &CPUstate.FQModule, sizeof(FQModule));
  memcpy(&ICacheModule, &CPUstate.ICacheModule, sizeof(ICacheModule));
  memcpy(&DecodeUnitModule, &CPUstate.DecodeUnitModule,
         sizeof(DecodeUnitModule));
  memcpy(&PRFModule, &CPUstate.PRFModule, sizeof(PRFModule));
  memcpy(&BPUModule, &CPUstate.BPUModule, sizeof(BPUModule));
  IMEMModule.snapshotFrom(CPUstate.IMEMModule);
  memcpy(&flushArbiter, &CPUstate.flushArbiter, sizeof(flushArbiter));
  memcpy(&FetchUnitModule, &CPUstate.FetchUnitModule,
         sizeof(FetchUnitModule));
  DMEMModule.snapshotFrom(CPUstate.DMEMModule);
  squashDetect = CPUstate.flushArbiter.arbitResult();
  fetchDecision = FetchDecision::build(
      BPUModule, FetchUnitModule.getPC(), squashDetect,
      FetchUnitModule.isHaltFetched(), FQModule.isFull(),
      ICacheModule.isRequestFull() || IMEMModule.isRequestFull());
  // FetchUnit halt signal: latch when the ICache head holds the halt
  // instruction (combinational bus)
  {
    bool haltSignal =
        ICacheModule.isReturnReady() && ICacheModule.returnRaw() == 0x0ff00513;
    fetchUnitInput.squashDetect = squashDetect;
    fetchUnitInput.fetchDecision = fetchDecision;
    fetchUnitInput.haltSignal = haltSignal;
  }
  // ICache hit check (comb, read snapshot ICacheModule) -> gate IMEM miss request
  // The line-return and FQ-consume handshakes are combinational predicates,
  // each side clears its own state (write-own-only)
  {
    bool icacheHit = fetchDecision.valid && ICacheModule.hit(fetchDecision.pc);
    FetchDecision imemFetch = fetchDecision;
    if (icacheHit)
      imemFetch.valid = false; // hit: no IMEM line fetch needed
    else if (fetchDecision.valid)
      imemFetch.pc = fetchDecision.pc & ~0xF; // line-aligned block addr for IMEM
    LineReturn lineReturn = IMEMModule.getReturn();
    imemInput.squashDetect = squashDetect;
    imemInput.fetchDecision = imemFetch;
    imemInput.lineConsumed = lineReturn.valid;
    icacheInput.squashDetect = squashDetect;
    icacheInput.fetchDecision = fetchDecision;
    icacheInput.lineReturn = lineReturn;
    bool popConsume = ICacheModule.isReturnReady() &&
                      !FetchUnitModule.isHaltFetched() && !FQModule.isFull();
    icacheInput.popConsume = popConsume;
  }
  fqInput.squashDetect = squashDetect;
  fqInput.haltFetched = FetchUnitModule.isHaltFetched();
  bpInput.fetchDecision = fetchDecision;
  cdbOut = CDBArbiter::build(ALUModule, LQModule, squashDetect);
  DispatchBus dispatchBus = DispatchArbiter::arbitrate(
      RSModule, ALUModule, AGUModule, BRUModule, ROBModule, PRFModule,
      squashDetect);
  aguInput.squashDetect = squashDetect;
  aluInput.squashDetect = squashDetect;
  aluInput.cdbOut = cdbOut;
  aluInput.dispatch = dispatchBus.alu;
  aguInput.dispatch = dispatchBus.agu;
  bruInput.dispatch = dispatchBus.bru;
  rsInput.dispatchBus = dispatchBus;
  dmemInput.squashDetect = squashDetect;
  decodeInput.squashDetect = squashDetect;
  lqInput.squashDetect = squashDetect;
  sqInput.squashDetect = squashDetect;
  auto memDispatch = MemRequestArbiter::arbitrate(LQModule, SQModule, ROBModule,
                                                  DMEMModule, squashDetect);
  dmemInput.decision = memDispatch;
  lqInput.decision = memDispatch;
  sqInput.decision = memDispatch;
  // store value-ready broadcast (data event): scan ready storeValueRS entries
  // against the SQ snapshot (store address must already be known)
  for (int i = 0; i < STORERS_CAP; ++i) {
    lqInput.storeNotifies[i] = StoreNotify{};
    if (!RSModule.storeValueRS[i].free &&
        PRFModule.isOperandReady(RSModule.storeValueRS[i].data)) {
      auto tag = RSModule.storeValueRS[i].robTag;
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash &&
           ROB::isOlder(tag, squashDetect.SquashTag))) {
        lqInput.storeNotifies[i] = SQModule.planDataForward(
            memSlot(RSModule.storeValueRS[i].memIndex),
            PRFModule.getOperandValue(RSModule.storeValueRS[i].data));
      }
    }
  }
  // store address-ready broadcast (address event): AGU head is a store
  lqInput.storeAddrNotify = StoreNotify{};
  if (!AGUModule.isEmpty() && isStoreMem(AGUModule.headMemIndex())) {
    auto aguRobTag = AGUModule.headRobTag();
    if (!squashDetect.needSquash ||
        (squashDetect.needSquash &&
         ROB::isOlder(aguRobTag, squashDetect.SquashTag))) {
      lqInput.storeAddrNotify = SQModule.planAddressForward(
          memSlot(AGUModule.headMemIndex()),
          static_cast<uint32_t>(AGUModule.headValue()));
    }
  }
  rsInput.squashDetect = squashDetect;
  robInput.squashDetect = squashDetect;
  robInput.cdbOut = cdbOut;
  prfInput.squashDetect = squashDetect;
  prfInput.cdbOut = cdbOut;
  ratInput.squashDetect = squashDetect;
  flarbInput.squashDetect = squashDetect;
  flarbInput.cdbOut = cdbOut;
  isarbInput.squashDetect = squashDetect;
  issuePacket = IssueArbiter::build(isarbInput);
  bruInput.squashDetect = squashDetect;
  bpInput.squashDetect = squashDetect;
  bpInput.cdbOut = cdbOut;
  CDBBus cdbBus = CDBBus::build(cdbOut, squashDetect);
  lqInput.cdbBus = cdbBus;
  auto LoadResponse = DMEMModule.LoadReturn(squashDetect);
  lqInput.loadResp = LoadResponse;
}

void CPU::run() {
  bool finish = false;
  uint64_t clock = 0;
  while (!finish) {
    comb();
    IMEMModule.tick(imemInput, CPUstate);
    FetchUnitModule.tick(fetchUnitInput, CPUstate);
    ICacheModule.tick(icacheInput, CPUstate);
    FQModule.tick(fqInput, CPUstate);
    LQModule.tick(lqInput, CPUstate);
    SQModule.tick(sqInput, CPUstate);
    ROBModule.tick(robInput, CPUstate);
    PRFModule.tick(prfInput, CPUstate);
    ALUModule.tick(aluInput, CPUstate);
    AGUModule.tick(aguInput, CPUstate);
    BRUModule.tick(bruInput, CPUstate);
    BPUModule.tick(bpInput, CPUstate);
    DMEMModule.tick(dmemInput, CPUstate);
    RSModule.tick(rsInput, CPUstate);
    RATModule.tick(ratInput, CPUstate);
    flushArbiter.tick(flarbInput, CPUstate);
    DecodeUnitModule.tick(decodeInput, CPUstate);
    ++clock;
    finish = ROBModule.isHaltCommitted() && FQModule.isEmpty() &&
             DecodeUnitModule.isEmpty() && ROBModule.isEmpty();
  }
  if (debug::enabled(debug::TOPIC_CLOCK))
    debug::print("clock: %llu\n", clock);
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n",
                 CPUstate.BPUModule.getBranchCorrect(),
                 CPUstate.BPUModule.getBranchTotal(),
                 CPUstate.BPUModule.getBranchTotal()
                     ? 100.0 * CPUstate.BPUModule.getBranchCorrect() /
                           CPUstate.BPUModule.getBranchTotal()
                     : 0.0);
  std::cout << std::dec
            << (PRFModule.getValue(
                    RATModule.readRAT_PRF(ROBModule.getHaltRd())) &
                 0xFF)
            << std::endl;
}