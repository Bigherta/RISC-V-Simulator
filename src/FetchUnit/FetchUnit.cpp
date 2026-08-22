#include "../include/FetchUnit.hpp"
#include "../include/CPU.hpp"

void FetchUnit::clear() {
  programCounter = 0;
  haltFetched = false;
}

void FetchUnit::setPC(uint32_t pc) { programCounter = pc; }

void FetchUnit::setHaltFetched(bool v) { haltFetched = v; }

void FetchUnit::tick(const FetchUnitInput &input, systemState &CPUstate) {
  // stage 3 flush first: on squash restore the PC and clear the halt flag
  if (input.squashDetect.needSquash) {
    CPUstate.FetchUnitModule.setPC(input.squashDetect.SquashPC);
    CPUstate.FetchUnitModule.setHaltFetched(false);
    return;
  }
  // stage 2 halt signal: latch when the ICache head holds the halt instruction
  if (input.haltSignal) {
    CPUstate.FetchUnitModule.setHaltFetched(true);
  }
  // stage 1 normal advance: advance the PC when fetchDecision is valid
  if (input.fetchDecision.valid) {
    CPUstate.FetchUnitModule.setPC(static_cast<uint32_t>(input.fetchDecision.predictedPC));
  }
}
