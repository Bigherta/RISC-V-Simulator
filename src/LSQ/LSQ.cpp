#include "../include/LSQ.hpp"
#include <algorithm>
#include <stdexcept>

bool LSQ::isEmpty() const { return tail == head; }

bool LSQ::isFull() const { return ((tail + 1) & 0x3F) == head; }

void LSQ::pop() { head = (head + 1) & 0x3F; }

void LSQ::pushLoad(int robIndex, uint64_t robSeq, int n_bytes,
                   bool isUnsigned) {
  LSQqueue[tail] = {};
  LSQqueue[tail].isLoad = true;
  LSQqueue[tail].robIndex = robIndex;
  LSQqueue[tail].robSeq = robSeq;
  LSQqueue[tail].n_bytes = n_bytes;
  LSQqueue[tail].isUnsigned = isUnsigned;
  LSQqueue[tail].isAddressReady = false;
  LSQqueue[tail].valueState = ValueState::NOTREADY;
  tail = (tail + 1) & 0x3F;
}

void LSQ::pushStore(int robIndex, uint64_t robSeq, int n_bytes) {
  LSQqueue[tail] = {};
  LSQqueue[tail].isLoad = false;
  LSQqueue[tail].robIndex = robIndex;
  LSQqueue[tail].robSeq = robSeq;
  LSQqueue[tail].n_bytes = n_bytes;
  LSQqueue[tail].isAddressReady = false;
  LSQqueue[tail].valueState = ValueState::NOTREADY;
  tail = (tail + 1) & 0x3F;
}

uint8_t LSQ::getHead() const { return head; }
uint8_t LSQ::getTail() const { return tail; }

int LSQ::getIndexBySeq(uint64_t robSeq) const {
  for (uint8_t cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (LSQqueue[cur].robSeq == robSeq)
      return cur;
  }
  return -1;
}

void LSQ::flush(uint8_t tailSnapshot) { tail = tailSnapshot; }

void LSQ::writeAddress(uint32_t address, int index) {
  LSQqueue[index].address = address;
  LSQqueue[index].isAddressReady = true;
}

void LSQ::writeValue(int32_t value, int index) {
  LSQqueue[index].value = value;
  LSQqueue[index].valueState = ValueState::READY;
  LSQqueue[index].isCDBBroadcast = false;
}

void LSQ::setValueState(int index, ValueState state) {
  LSQqueue[index].valueState = state;
}

void LSQ::setCDBBroadcast(int index) { LSQqueue[index].isCDBBroadcast = true; }

auto LSQ::getIsCDBBroadcast(int index) const -> bool {
  return LSQqueue[index].isCDBBroadcast;
}

auto LSQ::getAddress(int index) const -> uint32_t {
  if (LSQqueue[index].isAddressReady)
    return LSQqueue[index].address;
  throw std::runtime_error("Address is not ready!");
}

auto LSQ::getValue(int index) const -> int32_t {
  if (LSQqueue[index].valueState == ValueState::READY)
    return LSQqueue[index].value;
  throw std::runtime_error("Value is not ready!");
}

auto LSQ::isHeadLoad() const -> bool { return LSQqueue[head].isLoad; }

auto LSQ::headRobIndex() const -> int { return LSQqueue[head].robIndex; }

auto LSQ::headAddress() const -> uint32_t { return LSQqueue[head].address; }

auto LSQ::headValue() const -> int32_t { return LSQqueue[head].value; }

auto LSQ::headRobSeq() const -> uint64_t { return LSQqueue[head].robSeq; }

auto LSQ::headIsUnsigned() const -> bool { return LSQqueue[head].isUnsigned; }

auto LSQ::headNBytes() const -> int { return LSQqueue[head].n_bytes; }

auto LSQ::getRobIndex(int index) const -> int {
  return LSQqueue[index].robIndex;
}

auto LSQ::getRobSeq(int index) const -> uint64_t {
  return LSQqueue[index].robSeq;
}

auto LSQ::getIsLoad(int index) const -> bool { return LSQqueue[index].isLoad; }

auto LSQ::getIsUnsigned(int index) const -> bool {
  return LSQqueue[index].isUnsigned;
}

auto LSQ::getNBytes(int index) const -> int { return LSQqueue[index].n_bytes; }

auto LSQ::planDataForward(int index,
                          int32_t value) const -> LSQStoreToLoadForwardPlan {
  LSQStoreToLoadForwardPlan plan{};
  if (!LSQqueue[index].isAddressReady)
    return plan;
  if (index == ((tail - 1) & 0x3F))
    return plan;
  int unknownBiggestStore = index;
  int knownBiggestSameAddrStore = index;
  for (int i = (index + 1) & 0x3F; i != tail; i = (i + 1) & 0x3F) {
    if (LSQqueue[i].isLoad && LSQqueue[i].address == LSQqueue[index].address) {
      plan.writes[plan.count++] = {
          static_cast<uint8_t>(i), value,
          std::max(LSQqueue[i].knownBiggestStoreSeq,
                   LSQqueue[knownBiggestSameAddrStore].robSeq),
          unknownBiggestStore == index && knownBiggestSameAddrStore == index};
    } else if (!LSQqueue[i].isLoad) {
      if (!LSQqueue[i].isAddressReady) {
        unknownBiggestStore = i;
      } else if (LSQqueue[i].address == LSQqueue[index].address) {
        knownBiggestSameAddrStore = i;
      }
    }
  }
  return plan;
}

auto LSQ::planAddressForward(int index, uint32_t address) const
    -> LSQStoreToLoadForwardPlan {
  LSQStoreToLoadForwardPlan plan{};
  if (!LSQqueue[index].isLoad) {
    if (index == ((tail - 1) & 0x3F))
      return plan;
    int unknownBiggestStore = index;
    int knownBiggestSameAddrStore = index;
    for (int i = (index + 1) & 0x3F; i != tail; i = (i + 1) & 0x3F) {
      if (LSQqueue[i].isLoad && LSQqueue[i].address == address) {
        plan.writes[plan.count++] = {
            static_cast<uint8_t>(i), LSQqueue[index].value,
            std::max(LSQqueue[i].knownBiggestStoreSeq,
                     LSQqueue[knownBiggestSameAddrStore].robSeq),
            unknownBiggestStore == index &&
                knownBiggestSameAddrStore == index &&
                LSQqueue[index].valueState == ValueState::READY};
      } else if (!LSQqueue[i].isLoad) {
        if (!LSQqueue[i].isAddressReady) {
          unknownBiggestStore = i;
        } else if (LSQqueue[i].address == address) {
          knownBiggestSameAddrStore = i;
        }
      }
    }
  } else {
    int unknownBiggestStore = index;
    for (int i = index; i != ((head - 1) & 0x3F); i = (i + 63) & 0x3F) {
      if (!LSQqueue[i].isLoad) {
        if (LSQqueue[i].isAddressReady && LSQqueue[i].address == address) {
          plan.writes[plan.count++] = {static_cast<uint8_t>(index),
                                       LSQqueue[i].value, LSQqueue[i].robSeq,
                                       unknownBiggestStore == index &&
                                           LSQqueue[i].valueState ==
                                               ValueState::READY};
          break;
        } else if (!LSQqueue[i].isAddressReady) {
          unknownBiggestStore = i;
        }
      }
    }
  }
  return plan;
}

void LSQ::applyStoreToLoadForward(LSQStoreToLoadForwardPlan plan) {
  for (uint8_t k = 0; k < plan.count; ++k) {
    LSQWrite w = plan.writes[k];
    LSQqueue[w.index].knownBiggestStoreSeq = w.knownSeq;
    if (w.setValue)
      writeValue(w.value, w.index);
  }
}

int LSQ::LoadDetect() const {
  bool hasUnknownStore = false;
  if (head == tail)
    return 0xFFFFFFFF;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (!LSQqueue[cur].isLoad) {
      if (!LSQqueue[cur].isAddressReady) {
        hasUnknownStore = true;
      }
    } else if (LSQqueue[cur].isAddressReady &&
               LSQqueue[cur].valueState == ValueState::NOTREADY) {
      // Load with known address, value not yet dispatched to memory
      if (hasUnknownStore)
        break;
      if (LSQqueue[cur].valueState == ValueState::NOTREADY) {
        bool hasPendingSameAddrStore = false;
        for (int i = head; i != cur; i = (i + 1) & 0x3F) {
          if (!LSQqueue[i].isLoad && LSQqueue[i].isAddressReady &&
              LSQqueue[i].address == LSQqueue[cur].address) {
            hasPendingSameAddrStore = true;
            break;
          }
        }
        if (hasPendingSameAddrStore)
          break;
        // Still not resolved but no jeopardizing store, can load from DMEM
        return cur;
      }
    }
  }
  return 0xFFFFFFFF;
}

int LSQ::CDBDetect() const {
  if (head == tail)
    return 0xFFFFFFFF;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (LSQqueue[cur].isLoad) {
      if (LSQqueue[cur].isAddressReady &&
          LSQqueue[cur].valueState == ValueState::READY &&
          !LSQqueue[cur].isCDBBroadcast) {
        return cur;
      }
    }
  }
  return 0xFFFFFFFF;
}

bool LSQ::isReadyToCommit(int index) const {
  if (LSQqueue[index].isAddressReady &&
      LSQqueue[index].valueState == ValueState::READY) {
    return true;
  }
  return false;
}
