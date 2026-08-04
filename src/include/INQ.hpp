#pragma once
#ifndef INQ_HPP
#define INQ_HPP
#include "common.hpp"
#include <cstring>
#include <cstdint>
struct INQEntry {
  uint32_t raw;
  int pc;
  Instruct ninst;
  bool decoded;
};

class INQ {
private:
  INQEntry INQqueue[INQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  INQ() { std::memset(this, 0, sizeof(*this)); }
  bool isFull() const;
  bool isEmpty() const;
  void push(uint32_t raw, int pc);
  Instruct peek() const;
  Instruct pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  int decodeDetect() const;
  void decode(int index);
  void clear();
  bool headDecoded() const;
  INQEntry getEntry(int index) const;
};
#endif // INQ_HPP
