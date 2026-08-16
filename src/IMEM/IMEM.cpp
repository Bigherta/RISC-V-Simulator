#include "../include/IMEM.hpp"
#include "../include/CPU.hpp"
#include <cassert>
void IMEM::clear() {
  memset(IMEMreqs, 0, sizeof(IMEMreqs));
  head = count = 0;
}
void IMEM::snapshotFrom(const IMEM &other) {
  memcpy(IMEMreqs, other.IMEMreqs, sizeof(IMEMreqs));
  head = other.head;
  count = other.count;
  programCounter = other.programCounter;
  haltFetched = other.haltFetched;
}
void IMEM::pop() {
  IMEMreqs[head].valid = false;
  IMEMreqs[head].remain_cycle = 0;
  head = (head + 1) & (IMEM_CAP - 1);
  count--;
}
void IMEM::pushRequest(uint32_t pc, int32_t predictPC,
                       const BranchPredictorSnapshot &bpckpt) {
  assert(count != IMEM_CAP);
  IMEMRequest request{};
  request.remain_cycle = 3;
  request.ckpt = bpckpt;
  request.PC = pc;
  request.predictPC = predictPC;
  request.valid = true;
  IMEMreqs[(head + (count++)) & (IMEM_CAP - 1)] = request;
}
void IMEM::tick(const IMEMInput &input, systemState &CPUstate) {
  if (input.squashDetect.needSquash) {
    CPUstate.IMEMModule.clear();
    CPUstate.IMEMModule.programCounter = input.squashDetect.SquashPC;
    CPUstate.IMEMModule.haltFetched = false;
    return;
  }
  if (isReturnReady()) {
    if (haltFetched) {
      CPUstate.IMEMModule.pop();
    } else if (!input.FQModule.isFull()) {
      uint32_t raw = returnRaw();
      if (raw == 0x0ff00513)
        CPUstate.IMEMModule.haltFetched = true;
      CPUstate.IMEMModule.pop();
    }
  }
  if (input.fetchDecision.valid) {
    CPUstate.IMEMModule.pushRequest(input.fetchDecision.pc,
        input.fetchDecision.predictedPC, input.fetchDecision.ckpt);
    CPUstate.IMEMModule.programCounter = input.fetchDecision.predictedPC;
  }
  for (int i = 0; i < IMEM_CAP; ++i) {
    const auto &sreq = IMEMreqs[i];
    if (sreq.valid && sreq.remain_cycle > 0) {
      if (--CPUstate.IMEMModule.IMEMreqs[i].remain_cycle == 0)
        CPUstate.IMEMModule.IMEMreqs[i].raw_inst = read_inst(sreq.PC);
    }
  }
}