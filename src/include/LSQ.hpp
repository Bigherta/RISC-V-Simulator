#pragma once
#ifndef LSQ_HPP
#define LSQ_HPP
#include "common.hpp"
#include <cstdint>
#include <cstring>
enum class ValueState {
  NOTREADY,
  FETCHING,
  READY,
};
struct LSQEntry {
  Operation type;
  int ROBTag;
  uint32_t address;
  int32_t value;
  int n_bytes;
  int knownBiggestStoreTag;
  bool isAddressReady;
  ValueState valueState;
  bool isUnsigned;
  bool isCDBBroadcast;
};
struct LSQWrite {
  uint8_t index;
  int32_t value;
  int knownTag;
  bool setValue;
};
struct LSQStoreToLoadForwardPlan {
  LSQWrite writes[LSQ_CAP];
  uint8_t count;
};

class LSQ {
private:
  LSQEntry LSQqueue[LSQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  // Zero-initialize the whole object (incl. padding) so that byte-level
  // comparisons of copied states are deterministic.
  LSQ() { std::memset(this, 0, sizeof(*this)); }
  bool isEmpty() const;
  bool isFull() const;
  void push(LSQEntry entry);
  LSQEntry peek() const;
  LSQEntry pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  bool isReadyToCommit(int index) const;
  int getIndex(int ROBTag) const;
  void writeAddress(uint32_t address, int index);
  void writeValue(int32_t value, int index);
  auto getAddress(int index) const -> uint32_t;
  auto getValue(int index) const -> int32_t;
  auto getEntry(int index) const -> LSQEntry;
  void setValueState(int index, ValueState state);
  void setCDBBroadcast(int index);
  auto planDataForward(int index, int32_t value) const
      -> LSQStoreToLoadForwardPlan;
  auto planAddressForward(int index, uint32_t address) const
      -> LSQStoreToLoadForwardPlan;
  void applyStoreToLoadForward(LSQStoreToLoadForwardPlan plan);
  int CDBDetect() const;
  int LoadDetect() const;
  void flush(int tag);
};

#endif // LSQ_HPP