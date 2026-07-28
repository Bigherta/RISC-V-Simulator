#include "../include/Memory.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sys/types.h>
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
        addressMap[cur_address++] = hex2uint32(2, hex_byte);
        while (std::cin.peek() == '\n' || std::cin.peek() == ' ') {
          std::cin.get();
        }
        if (std::cin.peek() == '@')
          break;
      }
    }
  }
}

uint8_t Memory::read_data(uint32_t address)
{
    if (!addressMap.count(address)) {
        return 0;
    }
    return addressMap[address];
}

void Memory::write_data(uint32_t address, uint8_t data) {
  addressMap[address] = data;
}