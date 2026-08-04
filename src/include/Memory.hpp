#pragma once
#ifndef MEMORY_HPP
#define MEMORY_HPP
#include "common.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

static constexpr uint32_t MEM_SIZE = 256 * 1024;

class Memory {
private:
  uint8_t* mem;
  bool busy = false;
  bool bufferValid = false;
  MemRequest MemExecution;
  MemRequest MemOutputBuffer;

public:
  Memory() : mem(new uint8_t[MEM_SIZE]()) {}
  ~Memory() { delete[] mem; }
  Memory(const Memory& o)
    : mem(new uint8_t[MEM_SIZE]), busy(o.busy), bufferValid(o.bufferValid),
      MemExecution(o.MemExecution), MemOutputBuffer(o.MemOutputBuffer) {
    std::memcpy(mem, o.mem, MEM_SIZE);
  }
  Memory& operator=(const Memory& o) {
    if (this != &o) {
      std::memcpy(mem, o.mem, MEM_SIZE);
      busy = o.busy; bufferValid = o.bufferValid;
      MemExecution = o.MemExecution; MemOutputBuffer = o.MemOutputBuffer;
    }
    return *this;
  }

  void snapshotFrom(const Memory& o) {
    busy = o.busy; bufferValid = o.bufferValid;
    MemExecution = o.MemExecution; MemOutputBuffer = o.MemOutputBuffer;
  }
  void load_ins();
  inline uint32_t hex2uint32(int len, char hex[]);
  uint8_t read_data(uint32_t addr) const {
    if (addr >= MEM_SIZE)
      throw std::runtime_error("Memory read out of bounds");
    return mem[addr];
  }
  void write_data(uint32_t addr, uint8_t data) {
    if (addr >= MEM_SIZE)
      throw std::runtime_error("Memory write out of bounds");
    mem[addr] = data;
  }
  int32_t load_n_bytes(uint32_t address, int n, bool isSigned);
  void store_n_bytes(uint32_t address, int value, int n);
  bool MemPush(MemRequest request);
  void MemPull();
  void execute();
  MemRequest MemReturn() const;
  MemRequest MemExecutionState() const;
  bool isBusy() const;
  bool isReady() const;
  bool operator==(const Memory &other) const {
    return std::memcmp(mem, other.mem, MEM_SIZE) == 0;
  }
};
inline uint32_t Memory::hex2uint32(int len, char hex[]) {
  uint32_t result = 0;
  for (int i = 0; i < len; i++) {
    result *= 0x10;
    uint32_t value =
        (hex[i] >= '0' && hex[i] <= '9') ? (hex[i] - '0') : (hex[i] - 'A' + 10);
    result += value;
  }
  return result;
}
#endif // MEMORY_HPP