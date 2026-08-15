#include "../include/IMEM.hpp"
#include "../include/CPU.hpp"
void IMEM::clear() {
  memset(IMEMBuffer, 0, sizeof(IMEMBuffer));
  head = count = 0;
}

void IMEM::work(const IMEMInput &input, systemState &CPUstate) {
  if (input.squashDetect.needSquash) {
    CPUstate.IMEMModule.clear();
    CPUstate.programCounter = input.squashDetect.SquashPC;
    CPUstate.haltFetched = false;
    return;
  }
  if (!input.haltFetchRequest) {

  }
  auto headRequest = IMEMBuffer[head];
  headRequest.remain_cycle--;
  if(!headRequest.remain_cycle){
    CPUstate.IMEMModule.IMEMBuffer[head].raw_inst = read_inst(headRequest.PC);
  }
}