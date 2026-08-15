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
#include "FetchQueue.hpp"
struct DecodeInput {
  SquashInfo squashDetect;
  const FetchQueue &FQModule;
  DecodeInput(const FetchQueue &fq) : FQModule(fq) {}
};
struct systemState;
class DecodeUnit {
  friend struct ReorderTester;

private:
  InstructQueue iq;
  void push(Uop inst) { iq.push(inst); }

public:
  void tick(const DecodeInput &input, systemState &CPUstate);
  bool isEmpty() const { return iq.isEmpty(); }
  bool isFull() const { return iq.isFull(); }
  const Uop &headUop() const { return iq.headUop(); }
  void pop() { iq.pop(); }
  void clear() { iq.clear(); }
  uint8_t getHead() const { return iq.getHead(); }
  uint8_t getTail() const { return iq.getTail(); }
};
#endif // DECODER_HPP