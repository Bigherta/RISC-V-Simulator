#pragma once
#include "FetchQueue.hpp"
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
  RISC_V headType() const { return instructQueueEntries[head].type; }
  int headOpcode() const { return instructQueueEntries[head].opcode; }
  int headFunct3() const { return instructQueueEntries[head].funct3; }
  int headFunct7() const { return instructQueueEntries[head].funct7; }
  int headRd() const { return instructQueueEntries[head].rd; }
  int headRs1() const { return instructQueueEntries[head].rs1; }
  int headRs2() const { return instructQueueEntries[head].rs2; }
  int32_t headImm() const { return instructQueueEntries[head].imm; }
  uint32_t headPc() const { return instructQueueEntries[head].pc; }
  bool headIsHalt() const { return instructQueueEntries[head].isHalt; }
  bool headAllocDest() const { return instructQueueEntries[head].allocDest; }
  int32_t headPredictedPC() const {
    return instructQueueEntries[head].predictedPC;
  }
  uint8_t headCkptId() const { return instructQueueEntries[head].ckptId; }
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  void clear();
};
struct IssuePacket;
struct DecodeInput {
  SquashInfo squashDetect;
  const FetchQueue &FQModule;
  const IssuePacket &issuePacket;
  DecodeInput(const FetchQueue &fq, const IssuePacket &pkt)
      : FQModule(fq), issuePacket(pkt) {}
};
struct systemState;
class DecodeUnit {
  friend struct ReorderTester;

private:
  InstructQueue iq;
  void push(Uop inst) { iq.push(inst); }
  void pop() { iq.pop(); }
  void clear() { iq.clear(); }

public:
  void tick(const DecodeInput &input, systemState &CPUstate);
  bool isEmpty() const { return iq.isEmpty(); }
  bool isFull() const { return iq.isFull(); }
  RISC_V headType() const { return iq.headType(); }
  int headOpcode() const { return iq.headOpcode(); }
  int headFunct3() const { return iq.headFunct3(); }
  int headFunct7() const { return iq.headFunct7(); }
  int headRd() const { return iq.headRd(); }
  int headRs1() const { return iq.headRs1(); }
  int headRs2() const { return iq.headRs2(); }
  int32_t headImm() const { return iq.headImm(); }
  uint32_t headPc() const { return iq.headPc(); }
  bool headIsHalt() const { return iq.headIsHalt(); }
  bool headAllocDest() const { return iq.headAllocDest(); }
  int32_t headPredictedPC() const { return iq.headPredictedPC(); }
  uint8_t headCkptId() const { return iq.headCkptId(); }
  uint8_t getHead() const { return iq.getHead(); }
  uint8_t getTail() const { return iq.getTail(); }
};