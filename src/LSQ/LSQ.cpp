#include "../include/LSQ.hpp"
#include "../include/CPU.hpp"
#include "../include/ROB.hpp"
#include "../include/util.hpp"
#include <stdexcept>

bool LSQ::isEmpty() const { return tail == head; }

bool LSQ::isFull() const { return ((tail + 1) & 0x3F) == head; }

void LSQ::pop() { head = (head + 1) & 0x3F; }

void LSQ::pushLoad(RobTag robTag, int n_bytes, bool isUnsigned) {
  LSQqueue[tail] = {};
  LSQqueue[tail].isLoad = true;
  LSQqueue[tail].robTag = robTag;
  LSQqueue[tail].knownBiggestStoreValid = false;
  LSQqueue[tail].n_bytes = n_bytes;
  LSQqueue[tail].isUnsigned = isUnsigned;
  LSQqueue[tail].isAddressReady = false;
  LSQqueue[tail].valueState = ValueState::NOTREADY;
  tail = (tail + 1) & 0x3F;
}

void LSQ::pushStore(RobTag robTag, int n_bytes) {
  LSQqueue[tail] = {};
  LSQqueue[tail].isLoad = false;
  LSQqueue[tail].robTag = robTag;
  LSQqueue[tail].knownBiggestStoreValid = false;
  LSQqueue[tail].n_bytes = n_bytes;
  LSQqueue[tail].isAddressReady = false;
  LSQqueue[tail].valueState = ValueState::NOTREADY;
  tail = (tail + 1) & 0x3F;
}

uint8_t LSQ::getHead() const { return head; }
uint8_t LSQ::getTail() const { return tail; }

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

auto LSQ::headRobTag() const -> uint8_t { return LSQqueue[head].robTag; }

auto LSQ::getRobTag(int index) const -> uint8_t {
  return LSQqueue[index].robTag;
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
      auto candTag = LSQqueue[knownBiggestSameAddrStore].robTag;
      plan.writes[plan.count++] = {
          static_cast<uint8_t>(i), value,
          !LSQqueue[i].knownBiggestStoreValid
              ? candTag
              : (ROB::isOlder(LSQqueue[i].knownBiggestStoreTag, candTag)
                     ? candTag
                     : LSQqueue[i].knownBiggestStoreTag),
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
        auto candTag = LSQqueue[knownBiggestSameAddrStore].robTag;
        plan.writes[plan.count++] = {
            static_cast<uint8_t>(i), LSQqueue[index].value,
            !LSQqueue[i].knownBiggestStoreValid
                ? candTag
                : (ROB::isOlder(LSQqueue[i].knownBiggestStoreTag, candTag)
                       ? candTag
                       : LSQqueue[i].knownBiggestStoreTag),
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
                                       LSQqueue[i].value, LSQqueue[i].robTag,
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
    LSQqueue[w.index].knownBiggestStoreTag = w.knownTag;
    LSQqueue[w.index].knownBiggestStoreValid = true;
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

void LSQ::tick(const LSQInput &input, systemState &CPUstate) {
  // 0. issue apply: push the pre-built load/store entries
  const auto &p = input.issuePacket;
  if (p.valid && p.isLoad)
    CPUstate.LSQModule.pushLoad(p.robTag, p.nBytes, p.isUnsigned);
  if (p.valid && p.isStore)
    CPUstate.LSQModule.pushStore(p.robTag, p.nBytes);
  for (int i = 0; i < STORERS_CAP; ++i) {
    if (!input.RSModule.storeValueRS[i].free &&
        input.RSModule.storeValueRS[i].qrs2 == -1) {
      uint8_t SeqTag = input.RSModule.storeValueRS[i].robTag;
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(SeqTag, input.squashDetect.SquashTag))) {
        auto index = input.RSModule.storeValueRS[i].lsqIndex;
        auto plan = planDataForward(index, input.RSModule.storeValueRS[i].vrs2);
        CPUstate.LSQModule.writeValue(input.RSModule.storeValueRS[i].vrs2,
                                      index);
        CPUstate.LSQModule.applyStoreToLoadForward(plan);
      }
      CPUstate.RSModule.storeValueRS[i].free = true;
      CPUstate.RSModule.storeValueRS[i].qrs2 = -1;
    }
  }
  bool memBusy = input.DMEMModule.isBusy();
  bool storeDispatched = false;
  if (!memBusy && !isEmpty() && !isHeadLoad()) {
    auto storeTag = headRobTag();
    bool committed = input.ROBModule.isEmpty() ||
                     ROB::isOlder(storeTag, input.ROBModule.headTag());
    bool atHeadReady = !committed &&
                       headRobTag() == input.ROBModule.headTag() &&
                       input.ROBModule.isCommitReadyAt(
                           input.ROBModule.getIndexByTag(headRobTag()));
    if (committed || atHeadReady) {
      MemRequest newRequest{};
      newRequest.address = getAddress(getHead());
      newRequest.value = getValue(getHead());
      newRequest.isSigned = !getIsUnsigned(getHead());
      newRequest.n_bytes = getNBytes(getHead());
      newRequest.op = Operation::Store;
      newRequest.robTag = storeTag;
      newRequest.lsqIndex = head;
      if (CPUstate.DMEMModule.MemPush(newRequest)) {
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("LSQ store dispatch tag=%u @%u <- %d\n", storeTag,
                       newRequest.address, newRequest.value);
        if (debug::enabled(debug::TOPIC_MEM))
          debug::print("MEM store @%u <- %d\n", newRequest.address,
                       newRequest.value);
        storeDispatched = true;
        memBusy = true;
      }
    }
  }
  auto loadIndex = LoadDetect();
  if (loadIndex != 0xFFFFFFFF && !memBusy) {
    MemRequest newRequest{};
    newRequest.address = getAddress(loadIndex);
    newRequest.isSigned = !getIsUnsigned(loadIndex);
    newRequest.n_bytes = getNBytes(loadIndex);
    newRequest.op = Operation::Load;
    newRequest.robTag = getRobTag(loadIndex);
    newRequest.lsqIndex = loadIndex;
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(newRequest.robTag, input.squashDetect.SquashTag))) {
      if (CPUstate.DMEMModule.MemPush(newRequest)) {
        if (debug::enabled(debug::TOPIC_LSQ))
          debug::print("LSQ load dispatch tag=%u @%u\n", getRobTag(loadIndex),
                       newRequest.address);
        CPUstate.LSQModule.setValueState(loadIndex, ValueState::FETCHING);
      }
    }
  }
  uint8_t cur = getHead();
  bool retireLoad = !input.squashDetect.needSquash && cur != getTail() &&
                    isHeadLoad() &&
                    (input.ROBModule.isEmpty() ||
                     ROB::isOlder(headRobTag(), input.ROBModule.headTag()));
  (storeDispatched || retireLoad) ? CPUstate.LSQModule.pop() : void();

  // LSQ receive the value from AGU
  if (!input.AGUModule.isEmpty()) {
    auto aguRobTag = input.AGUModule.headRobTag();
    auto aguLsqIndex = input.AGUModule.headlsqIndex();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(aguRobTag, input.squashDetect.SquashTag))) {
        auto value = input.AGUModule.headValue();
        auto plan = planAddressForward(aguLsqIndex, value);
        CPUstate.LSQModule.writeAddress(static_cast<uint32_t>(value), aguLsqIndex);
        CPUstate.LSQModule.applyStoreToLoadForward(plan);
    }
  }
  // consume the CDB
  if (input.cdbBus.lsqSetCDB) {
      CPUstate.LSQModule.setCDBBroadcast(input.cdbBus.lsqIndex);
  }
  // clear the wrong LSQ (only on squash; SquashIndex is -1 otherwise)
  if (input.squashDetect.needSquash) {
    CPUstate.LSQModule.flush(
        input.ROBModule.getLsqTailSnapshot(input.squashDetect.SquashIndex));
  }
}
