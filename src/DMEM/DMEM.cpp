#include "../include/DMEM.hpp"
#include "../include/CPU.hpp"

void DMEM::snapshotFrom(const DMEM &other) {
  busy = other.busy;
  bufferValid = other.bufferValid;
  MemExecution = other.MemExecution;
  MemOutputBuffer = other.MemOutputBuffer;
}

int32_t DMEM::load_n_bytes(uint32_t address, int n, bool isSigned,
                           bool tolerant) const {
  int32_t result = 0;
  for (int i = 0; i < n; i++) {
    auto byte_data =
        (tolerant && address + i >= MEM_SIZE) ? 0 : read_data(address + i);
    result |= (byte_data << (i << 3));
    if (i == n - 1 && n < 4 && isSigned) {
      if (result & (1 << ((n << 3) - 1))) {
        auto mask = ~((1 << (n << 3)) - 1);
        result |= mask;
      }
    }
  }
  return result;
}

void DMEM::store_n_bytes(uint32_t address, int value, int n) {
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(value >> (i << 3));
    write_data(address + i, byte_data);
  }
}

void DMEM::MemPull() { bufferValid = false; }

LoadResponse DMEM::LoadReturn(const SquashInfo &squash) const {
  LoadResponse response;
  if (isReady()) {
    auto reply = MemOutputBuffer;
    if (reply.op == Operation::Load &&
        (!squash.needSquash || ROB::isOlder(reply.robTag, squash.SquashTag))) {
      response.valid = true;
      response.memIndex = reply.memIndex;
      response.robTag = reply.robTag;
      response.value = reply.value;
    }
  }
  return response;
}

bool DMEM::isBusy() const { return busy; }

bool DMEM::isReady() const { return bufferValid; }

void DMEM::tick(const DMEMInput &input, systemState &CPUstate) {
  // claim the pre-computed mem request (read = comb phase decided it)
  if (!busy && input.decision.valid) {
    CPUstate.DMEMModule.MemExecution = input.decision.request;
    CPUstate.DMEMModule.busy = true;
    if (input.decision.request.speculative)
      CPUstate.DMEMModule.speculativeLoads++;
  }
  // output stage: consume the previous cycle's reply
  if (isReady()) {
    CPUstate.DMEMModule.MemPull();
  }
  // execution stage: this=snapshot reads own registers (hardware FSM),
  // writes the active module through the edge-write handle
  if (!busy)
    return;
  MemRequest exec = MemExecution;
  exec.remainCycle--;
  if (!exec.remainCycle) {
    if (exec.op == Operation::Load) {
      exec.value = CPUstate.DMEMModule.load_n_bytes(
          exec.address, exec.n_bytes, exec.isSigned, exec.speculative);
    } else {
      CPUstate.DMEMModule.store_n_bytes(exec.address, exec.value, exec.n_bytes);
    }
    CPUstate.DMEMModule.MemOutputBuffer = exec;
    CPUstate.DMEMModule.bufferValid = true;
    CPUstate.DMEMModule.busy = false;
  } else {
    CPUstate.DMEMModule.MemExecution = exec;
    CPUstate.DMEMModule.busy = true;
  }
}
