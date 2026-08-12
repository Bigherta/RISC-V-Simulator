#pragma once
#ifndef DECODER_HPP
#define DECODER_HPP
#include "common.hpp"
#include <cstdint>

class Decoder {
public:
  static Uop decode(int32_t raw_inst);
  static inline int32_t signExtend(int32_t raw_data, int len) {
    return (raw_data << (32 - len)) >> (32 - len);
  }
};

class InstructQueue {
  friend struct ReorderTester;
private:
  Uop instructQueueEntries[IQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  bool isEmpty() const;
  bool isFull() const;
  void push(Uop inst);
  const Uop &headUop() const;
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  void clear();
};
#endif // DECODER_HPP