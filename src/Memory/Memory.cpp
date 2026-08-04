#include "../include/Memory.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

void Memory::load_ins() {
  while (!std::cin.eof()) {
    char sign = std::cin.get();
    if (sign == EOF) {
      return;
    }
    if (sign == '@') {
      char hex_address[9];
      std::cin >> hex_address;
      uint32_t cur_address = hex2uint32(8, hex_address);
      char hex_byte[3];
      while (std::cin >> hex_byte) {
        write_data(cur_address++, hex2uint32(2, hex_byte));
        while (std::cin.peek() == '\n' || std::cin.peek() == ' ') {
          std::cin.get();
        }
        if (std::cin.peek() == '@')
          break;
      }
    }
  }
}

bool Memory::MemPush(MemRequest request) {
  if (busy)
    throw std::runtime_error("DMEM is busy!");
  MemExecution = request;
  return busy = true;
}

void Memory::MemPull() { bufferValid = false; }

int32_t Memory::load_n_bytes(uint32_t address, int n, bool isSigned) {
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

void Memory::store_n_bytes(uint32_t address, int value, int n) {
  for (int i = 0; i < n; i++) {
    auto byte_data = static_cast<uint8_t>(value >> (i << 3));
    write_data(address + i, byte_data);
  }
}

void Memory::execute() {
  if (!busy)
    return;
  MemExecution.remainCycle--;
  if (!MemExecution.remainCycle) {
    if (MemExecution.op == Operation::Load) {
      MemExecution.value = load_n_bytes(
          MemExecution.address, MemExecution.n_bytes, MemExecution.isSigned);
    } else {
      store_n_bytes(MemExecution.address, MemExecution.value,
                    MemExecution.n_bytes);
    }
    MemOutputBuffer = MemExecution;
    bufferValid = true;
    busy = false;
  }
}

MemRequest Memory::MemReturn() const { return MemOutputBuffer; }

MemRequest Memory::MemExecutionState() const { return MemExecution; }

bool Memory::isBusy() const { return busy; }

bool Memory::isReady() const { return bufferValid; }