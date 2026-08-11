#pragma once
#ifndef PRF_HPP
#define PRF_HPP
#include <cstdint>
#include "common.hpp"
struct PRFEntry {
  int32_t value;
  bool ready;
};

class PRF {
  friend struct ReorderTester;
private:
  PRFEntry PhysicalRegs[PRF_CAP];
  uint8_t freeList[PRF_CAP];
  uint32_t headSeq = 0;   
  uint32_t tailSeq = 0;

public:
  PRF();
  uint8_t pop();                      
  void push(int index);
  bool isFreeListEmpty() const;     
  uint32_t getHeadSeq() const;
  uint32_t getTailSeq() const;
  uint8_t getFreeListSlot(uint32_t seq) const {
    return freeList[seq & (PRF_CAP - 1)];
  }
  void restoreHead(uint32_t ckptHeadSeq);   
  bool isReady(int index) const;            
  int32_t getValue(int index) const;
  void write(int index, int32_t value);
  uint32_t head() const { return headSeq & (PRF_CAP - 1); }   
  uint32_t tail() const { return tailSeq & (PRF_CAP - 1); }
  uint32_t size() const { return tailSeq - headSeq; }
};
#endif
