#include "../include/DMEM.hpp"
#include "../include/CPU.hpp"
#include "common.hpp"
#include <cstdint>

void DMEM::snapshotFrom(const DMEM &other) {
  readBusy = other.readBusy;
  readBufferValid = other.readBufferValid;
  writeBusy = other.writeBusy;
  readExecute = other.readExecute;
  writeExecute = other.writeExecute;
  readOutputBuffer = other.readOutputBuffer;
}

int32_t DMEM::load_n_bytes(uint32_t address, int n, bool isSigned) const {
  int32_t result = 0;
  for (int i = 0; i < n; i++) {
    auto byte_data = read_data(address + i);
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

void DMEM::writeLine(uint32_t addr, const uint8_t *lineData) {
  for (int i = 0; i < DCACHE_BLOCK_CAP; i++) {
    write_data(addr + i, lineData[i]);
  }
}

const uint8_t *DMEM::readLine(uint32_t addr) { return mem + addr; }

void DMEM::MemPull() { readBufferValid = false; }

bool DMEM::isReadBusy() const { return readBusy; }

bool DMEM::isWriteBusy() const { return writeBusy; }

bool DMEM::isReplyReady() const { return readBufferValid; }

void DMEM::tick(const DMEMInput &input, systemState &CPUstate) {
  // claim the pre-computed mem request (read = comb phase decided it)
    // read
    if (input.request.readValid  && !isReadBusy()) {
      CPUstate.DMEMModule.readExecute = input.request.read;
      CPUstate.DMEMModule.readBusy = true;
    }
    // write
    if (input.request.writeValid && !isWriteBusy()) {
      CPUstate.DMEMModule.writeExecute = input.request.write;
      CPUstate.DMEMModule.writeBusy = true;
    }
  // output stage: consume the previous cycle's reply
  if (isReplyReady()) {
    CPUstate.DMEMModule.MemPull();
  }
  // execution stage: this=snapshot reads own registers (hardware FSM),
  // writes the active module through the edge-write handle
  if (readBusy) {
    auto readExec = readExecute;
    readExec.remainCycle--;
    if (!readExec.remainCycle) {
      auto block_base = readExec.address & ~0xF;
      auto &out = CPUstate.DMEMModule.readOutputBuffer;
      // Read the LIVE instance's storage: snapshotFrom() deliberately does
      // NOT copy the 128KB mem array, so this->mem is a stale boot image.
      // writeLine lands dirty victims in CPUstate.DMEMModule.mem -- the fill
      // must observe them (same convention as IMEM's CPUstate.read_data).
      for (int i = 0; i < DCACHE_BLOCK_CAP; ++i)
        out.lineData[i] = CPUstate.DMEMModule.read_data(block_base + i);
      CPUstate.DMEMModule.readBufferValid = true;
      CPUstate.DMEMModule.readBusy = false;
    } else {
      CPUstate.DMEMModule.readExecute = readExec;
      CPUstate.DMEMModule.readBusy = true;
    }
  }
  if (writeBusy) {
    auto writeExec = writeExecute;
    writeExec.remainCycle--;
    if (!writeExec.remainCycle) {
      CPUstate.DMEMModule.writeLine(writeExec.address, writeExec.lineData);
      CPUstate.DMEMModule.writeBusy = false;
    } else {
      CPUstate.DMEMModule.writeExecute = writeExec;
      CPUstate.DMEMModule.writeBusy = true;
    }
  }
}
