#pragma once
#include <cstdint>
#include <unordered_map>
#ifndef MEMORY_HPP
#define MEMORY_HPP
class Memory {
private:
  std::unordered_map<uint32_t, uint8_t> addressMap;

public:
  void load_ins();
  inline uint32_t hex2uint32(int len, char hex[]);
  uint8_t read_data(uint32_t);
  void write_data(uint32_t, uint8_t);
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