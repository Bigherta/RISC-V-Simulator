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
  RobTag robTag;
  uint8_t knownBiggestStoreTag; // 已知最老同地址 store 的 tag（valid 配合）
  bool knownBiggestStoreValid; // 是否有已知 forwarding store（硬件 valid bit）
  uint32_t address;
  int32_t value;
  int n_bytes;
  bool isAddressReady;
  ValueState valueState;
  bool isUnsigned;
  bool isCDBBroadcast;
};
struct LSQWrite {
  uint8_t index;
  int32_t value;
  uint8_t knownTag;
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
struct IssuePacket;
struct LSQInput {
  SquashInfo squashDetect;
  const AGU &AGUModule;
  const RSUnit &RSModule;
  const ROB &ROBModule;
  const DMEM &DMEMModule;
  const IssuePacket &issuePacket;
  CDBBus cdbBus;
  LoadResponse loadResp;
  MemDispatchDecision decision;
  LSQInput(const AGU &agu, const RSUnit &rs, const ROB &rob, const DMEM &dmem,
           const IssuePacket &pkt)
      : AGUModule(agu), RSModule(rs), ROBModule(rob), DMEMModule(dmem),
        issuePacket(pkt) {}
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
  bool isActive(uint8_t index) const;
  void pushLoad(RobTag robTag, int n_bytes, bool isUnsigned);
  void pushStore(RobTag robTag, int n_bytes);
  void pop();
  uint8_t getHead() const;
  uint8_t getTail() const;
  bool isReadyToCommit(int index) const;
  void writeAddress(uint32_t address, int index);
  void writeValue(int32_t value, int index);
  auto getAddress(int index) const -> uint32_t;
  auto getValue(int index) const -> int32_t;
  auto isHeadLoad() const -> bool;
  auto headRobTag() const -> uint8_t;
  auto getRobTag(int index) const -> uint8_t;
  auto getIsLoad(int index) const -> bool;
  auto getIsUnsigned(int index) const -> bool;
  auto getNBytes(int index) const -> int;
  void setValueState(int index, ValueState state);
  void setCDBBroadcast(int index);
  auto getIsCDBBroadcast(int index) const -> bool;
  auto planDataForward(int index,
                       int32_t value) const -> LSQStoreToLoadForwardPlan;
  auto planAddressForward(int index,
                          uint32_t address) const -> LSQStoreToLoadForwardPlan;
  void applyStoreToLoadForward(LSQStoreToLoadForwardPlan plan);
  int CDBDetect() const;
  int LoadDetect() const;
  void flush(uint8_t tailSnapshot);
  MemDispatchDecision selectMemRequest(const ROB &rob, const DMEM &dmem,
                                       const SquashInfo &squash) const;
  void tick(const LSQInput &, systemState &);
};

#endif // LSQ_HPP