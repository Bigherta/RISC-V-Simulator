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
  int robIndex;
  uint64_t robSeq;
  uint32_t address;
  int32_t value;
  int n_bytes;
  uint64_t knownBiggestStoreSeq;
  bool isAddressReady;
  ValueState valueState;
  bool isUnsigned;
  bool isCDBBroadcast;
};
struct LSQWrite {
  uint8_t index;
  int32_t value;
  uint64_t knownSeq;
  bool setValue;
};
struct LSQStoreToLoadForwardPlan {
  LSQWrite writes[LSQ_CAP];
  uint8_t count;
};
struct systemState;
struct AGU;
struct RSUnit;
struct ROB;
struct DMEM;
struct LSQInput {
  SquashInfo squashDetect;
  const AGU &AGUModule;
  const RSUnit &RSModule;                       
  const ROB &ROBModule;                     
  const DMEM &DMEMModule;
  CDBBus cdbBus;                   
  LSQInput(const AGU &agu, const RSUnit &rs, const ROB &rob, const DMEM &dmem)
      : AGUModule(agu), RSModule(rs), ROBModule(rob), DMEMModule(dmem) {}
};
class LSQ {
  friend struct ReorderTester;
private:
  LSQEntry LSQqueue[LSQ_CAP];
  uint8_t head = 0;
  uint8_t tail = 0;

public:
  LSQ() { std::memset(this, 0, sizeof(*this)); }
  bool isEmpty() const;
  bool isFull() const;
  void pushLoad(int robIndex, uint64_t robSeq, int n_bytes, bool isUnsigned);
  void pushStore(int robIndex, uint64_t robSeq, int n_bytes);
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  bool isReadyToCommit(int index) const;
  int getIndexBySeq(uint64_t robSeq) const;
  void writeAddress(uint32_t address, int index);
  void writeValue(int32_t value, int index);
  auto getAddress(int index) const -> uint32_t;
  auto getValue(int index) const -> int32_t;
  auto isHeadLoad() const -> bool;
  auto headRobSeq() const -> uint64_t;
  auto getRobIndex(int index) const -> int;
  auto getRobSeq(int index) const -> uint64_t;
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
  void flush(uint8_t tailSnapshot);
  void tick(const LSQInput&, systemState&);
};

#endif // LSQ_HPP