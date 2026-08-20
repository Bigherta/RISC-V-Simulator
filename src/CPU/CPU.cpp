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
  memcpy(&DecodeUnitModule, &CPUstate.DecodeUnitModule,
         sizeof(DecodeUnitModule));
  memcpy(&PRFModule, &CPUstate.PRFModule, sizeof(PRFModule));
  memcpy(&BPModule, &CPUstate.BPModule, sizeof(BPModule));
  IMEMModule.snapshotFrom(CPUstate.IMEMModule);
  memcpy(&flushArbiter, &CPUstate.flushArbiter, sizeof(flushArbiter));
  DMEMModule.snapshotFrom(CPUstate.DMEMModule);
  squashDetect = CPUstate.flushArbiter.arbitResult();
  imemInput.squashDetect = squashDetect;
  fetchDecision = FetchDecision::build(
      BPModule, IMEMModule.getPC(), squashDetect, IMEMModule.isHaltFetched(),
      FQModule.isFull(), IMEMModule.isRequestFull());
  imemInput.fetchDecision = fetchDecision;
  fqInput.squashDetect = squashDetect;
  fqInput.haltFetched = IMEMModule.isHaltFetched();
  bpInput.fetchDecision = fetchDecision;
  cdbOut = CDBArbiter::build(ALUModule, LQModule, squashDetect);
  DispatchBus dispatchBus = DispatchArbiter::arbitrate(
      RSModule, ALUModule, AGUModule, BRUModule, ROBModule, squashDetect);
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
        RSModule.storeValueRS[i].qrs2 == -1) {
      auto tag = RSModule.storeValueRS[i].robTag;
      if (!squashDetect.needSquash ||
          (squashDetect.needSquash &&
           ROB::isOlder(tag, squashDetect.SquashTag))) {
        lqInput.storeNotifies[i] = SQModule.planDataForward(
            memSlot(RSModule.storeValueRS[i].memIndex),
            RSModule.storeValueRS[i].vrs2);
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
  isarbInput.cdbOut = cdbOut;
  issuePacket = IssueArbiter::build(isarbInput);
  bruInput.squashDetect = squashDetect;
  bpInput.squashDetect = squashDetect;
  bpInput.cdbOut = cdbOut;
  CDBBus cdbBus = CDBBus::build(cdbOut, ROBModule, PRFModule, squashDetect);
  lqInput.cdbBus = cdbBus;
  rsInput.cdbBus = cdbBus;
  auto LoadResponse = DMEMModule.LoadReturn(squashDetect);
  lqInput.loadResp = LoadResponse;
}

void CPU::run() {
  bool finish = false;
  uint64_t clock = 0;
  while (!finish) {
    comb();
    IMEMModule.tick(imemInput, CPUstate);
    FQModule.tick(fqInput, CPUstate);
    LQModule.tick(lqInput, CPUstate);
    SQModule.tick(sqInput, CPUstate);
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
    ++clock;
    finish = ROBModule.isHaltCommitted() && FQModule.isEmpty() &&
             DecodeUnitModule.isEmpty() && ROBModule.isEmpty();
  }
  if (debug::enabled(debug::TOPIC_CLOCK))
    debug::print("clock: %llu\n", clock);
  if (debug::enabled(debug::TOPIC_BRANCH))
    debug::print("branch: %llu/%llu correct (%.2f%%)\n",
                 CPUstate.BPModule.getBranchCorrect(),
                 CPUstate.BPModule.getBranchTotal(),
                 CPUstate.BPModule.getBranchTotal()
                     ? 100.0 * CPUstate.BPModule.getBranchCorrect() /
                           CPUstate.BPModule.getBranchTotal()
                     : 0.0);
  if (debug::enabled(debug::TOPIC_MDP))
    debug::print("mdp: violations=%llu speculative=%llu\n",
                 CPUstate.flushArbiter.getLoadViolations(),
                 CPUstate.DMEMModule.getSpeculativeLoads());
  std::cout << std::dec
            << (PRFModule.getValue(
                    RATModule.readRAT_PRF(ROBModule.getHaltRd())) &
                 0xFF)
            << std::endl;
}