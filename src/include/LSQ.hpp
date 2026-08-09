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
  bool isLoad;
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
  LSQ() { std::memset(this, 0, sizeof(*this)); }
  bool isEmpty() const;
  bool isFull() const;
  void pushLoad(int ROBTag, int n_bytes, bool isUnsigned);
  void pushStore(int ROBTag, int n_bytes);
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  bool isReadyToCommit(int index) const;
  int getIndex(int ROBTag) const;
  void writeAddress(uint32_t address, int index);
  void writeValue(int32_t value, int index);
  auto getAddress(int index) const -> uint32_t;
  auto getValue(int index) const -> int32_t;
  auto isHeadLoad() const -> bool;
  auto headROBTag() const -> int;
  auto headAddress() const -> uint32_t;
  auto headValue() const -> int32_t;
  auto headIsUnsigned() const -> bool;
  auto headNBytes() const -> int;
  auto getROBTag(int index) const -> int;
  auto getIsLoad(int index) const -> bool;
  auto getIsUnsigned(int index) const -> bool;
  auto getNBytes(int index) const -> int;
  void setValueState(int index, ValueState state);
  void setCDBBroadcast(int index);
  auto getIsCDBBroadcast(int index) const -> bool;
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