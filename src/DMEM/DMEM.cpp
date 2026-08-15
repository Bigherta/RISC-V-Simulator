#include "../include/DMEM.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"
#include <stdexcept>

void DMEM::snapshotFrom(const DMEM& other) {
  busy = other.busy;
  bufferValid = other.bufferValid;
  MemExecution = other.MemExecution;
  MemOutputBuffer = other.MemOutputBuffer;
}

int32_t DMEM::load_n_bytes(uint32_t address, int n, bool isSigned) {
  int32_t result = 0;
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint32_t>(read_data(address + i));
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

bool DMEM::MemPush(MemRequest request) {
  if (busy)
    throw std::runtime_error("DMEM is busy!");
  MemExecution = request;
  return busy = true;
}

void DMEM::MemPull() { bufferValid = false; }

MemRequest DMEM::MemReturn() const { return MemOutputBuffer; }

bool DMEM::isBusy() const { return busy; }

bool DMEM::isReady() const { return bufferValid; }

void DMEM::tick(const DMEMInput& input, systemState& next) {
  // output stage: consume the previous cycle's reply
  if (isReady()) {
    auto reply = MemReturn();
    if (reply.op == Operation::Load &&
        (!input.squashDetect.needSquash ||
         reply.robSeq < input.squashDetect.SquashSeq)) {
      auto index = input.LSQModule.getIndexBySeq(reply.robSeq);
      if (index >= 0) {
        if (debug::enabled(debug::TOPIC_MEM))
          debug::print("MEM load @%u <- %d\n", reply.address, reply.value);
        next.LSQModule.writeValue(reply.value, index);
      }
    }
    next.DMEMModule.MemPull();
  }
  // execution stage: this=snapshot reads own registers (hardware FSM),
  // writes the active module through the edge-write handle
  if (!busy)
    return;
  MemRequest exec = MemExecution;
  exec.remainCycle--;
  if (!exec.remainCycle) {
    if (exec.op == Operation::Load) {
      exec.value = next.DMEMModule.load_n_bytes(
          exec.address, exec.n_bytes, exec.isSigned);
    } else {
      next.DMEMModule.store_n_bytes(exec.address, exec.value, exec.n_bytes);
    }
    next.DMEMModule.MemOutputBuffer = exec;
    next.DMEMModule.bufferValid = true;
    next.DMEMModule.busy = false;
  } else {
    next.DMEMModule.MemExecution = exec;
    next.DMEMModule.busy = true;
  }
}
