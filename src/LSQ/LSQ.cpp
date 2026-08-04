#include "../include/LSQ.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

bool LSQ::isEmpty() const { return tail == head; }

bool LSQ::isFull() const { return ((tail + 1) & 0x3F) == head; }

void LSQ::push(LSQEntry entry) {
  LSQqueue[tail] = entry;
  tail = (tail + 1) & 0x3F;
}

LSQEntry LSQ::pop() {
  auto temp = LSQqueue[head];
  head = (head + 1) & 0x3F;
  return temp;
}

LSQEntry LSQ::peek() const {
  if (isEmpty())
    throw std::runtime_error("peek an empty LSQ!");
  return LSQqueue[head];
}

uint8_t LSQ::getHead() const { return head; }
uint8_t LSQ::getTail() const { return tail; }

int LSQ::getIndex(int ROBTag) const {
  for (int i = 0; i < LSQ_CAP; i++) {
    if (LSQqueue[i].ROBTag == ROBTag)
      return i;
  }
  return -1;
}

void LSQ::flush(int tag) {
  int first_flushed = -1;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (LSQqueue[cur].ROBTag > tag) {
      if (first_flushed == -1)
        first_flushed = cur;
    }
  }
  if (first_flushed != -1)
    tail = first_flushed;
}

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

void LSQ::setCDBBroadcast(int index) {
  LSQqueue[index].isCDBBroadcast = true;
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

auto LSQ::getEntry(int index) const -> LSQEntry {
  return LSQqueue[index];
}

void LSQ::DataBroadcast(int index) {
  if (!LSQqueue[index].isAddressReady)
    return;
  if (index == ((tail - 1) & 0x3F))
    return;
  int unknownBiggestStore = index;
  int knownBiggestSameAddrStore = index;
  for (int i = (index + 1) & 0x3F; i != tail; i = (i + 1) & 0x3F) {
    if (LSQqueue[i].type == Operation::Load &&
        LSQqueue[i].address == LSQqueue[index].address) {
      LSQqueue[i].knownBiggestStoreTag =
          std::max(LSQqueue[i].knownBiggestStoreTag,
                   LSQqueue[knownBiggestSameAddrStore].ROBTag);
      if (unknownBiggestStore == index && knownBiggestSameAddrStore == index) {
        writeValue(LSQqueue[index].value, i);
      }
    } else if (LSQqueue[i].type == Operation::Store) {
      if (!LSQqueue[i].isAddressReady) {
        unknownBiggestStore = i;
      } else if (LSQqueue[i].address == LSQqueue[index].address) {
        knownBiggestSameAddrStore = i;
      }
    }
  }
}

void LSQ::AddressBroadcast(int index) {
  if (LSQqueue[index].type == Operation::Store) {
    if (index == ((tail - 1) & 0x3F))
      return;
    int unknownBiggestStore = index;
    int knownBiggestSameAddrStore = index;
    for (int i = (index + 1) & 0x3F; i != tail; i = (i + 1) & 0x3F) {
      if (LSQqueue[i].type == Operation::Load &&
          LSQqueue[i].address == LSQqueue[index].address) {
        LSQqueue[i].knownBiggestStoreTag =
            std::max(LSQqueue[i].knownBiggestStoreTag,
                     LSQqueue[knownBiggestSameAddrStore].ROBTag);
        if (unknownBiggestStore == index &&
            knownBiggestSameAddrStore == index &&
            LSQqueue[index].valueState == ValueState::READY) {
          writeValue(LSQqueue[index].value, i);
        }
      } else if (LSQqueue[i].type == Operation::Store) {
        if (!LSQqueue[i].isAddressReady) {
          unknownBiggestStore = i;
        } else if (LSQqueue[i].address == LSQqueue[index].address) {
          knownBiggestSameAddrStore = i;
        }
      }
    }
  } else {
    int unknownBiggestStore = index;
    for (int i = index; i != ((head - 1) & 0x3F); i = (i + 63) & 0x3F) {
      if (LSQqueue[i].type == Operation::Store) {
        if (LSQqueue[i].isAddressReady &&
            LSQqueue[i].address == LSQqueue[index].address) {
          if (unknownBiggestStore == index &&
              LSQqueue[i].valueState == ValueState::READY) {
            writeValue(LSQqueue[i].value, index);
          }
          LSQqueue[index].knownBiggestStoreTag = LSQqueue[i].ROBTag;
          break;
        } else if (!LSQqueue[i].isAddressReady) {
          unknownBiggestStore = i;
        }
      }
    }
  }
}

int LSQ::LoadDetect() const {
  bool hasUnknownStore = false;
  if (head == tail)
    return 0xFFFFFFFF;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (LSQqueue[cur].type == Operation::Store) {
      if (!LSQqueue[cur].isAddressReady) {
        hasUnknownStore = true;
      }
    } else if (LSQqueue[cur].isAddressReady &&
               LSQqueue[cur].valueState == ValueState::NOTREADY) {
      // Load with known address, value not yet dispatched to memory
      if (hasUnknownStore)
        break;
      if (LSQqueue[cur].valueState == ValueState::NOTREADY) {
        // An older same-address store whose value is still pending would
        // overwrite this load later (via its DataBroadcast); dispatching to
        // memory now could read a stale value. Wait for the forwarding.
        bool hasPendingSameAddrStore = false;
        for (int i = head; i != cur; i = (i + 1) & 0x3F) {
          if (LSQqueue[i].type == Operation::Store &&
              LSQqueue[i].isAddressReady &&
              LSQqueue[i].address == LSQqueue[cur].address &&
              LSQqueue[i].valueState != ValueState::READY) {
            hasPendingSameAddrStore = true;
            break;
          }
        }
        if (hasPendingSameAddrStore)
          break;
        // Still not resolved — must go to memory
        return cur;
      }
      // If valueState changed to READY (forwarded from store), skip
    }
  }
  return 0xFFFFFFFF;
}

int LSQ::CDBDetect() const {
  if (head == tail)
    return 0xFFFFFFFF;
  for (int cur = head; cur != tail; cur = (cur + 1) & 0x3F) {
    if (LSQqueue[cur].type == Operation::Load) {
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