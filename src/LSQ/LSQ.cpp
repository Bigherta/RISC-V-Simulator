#include "../include/LSQ.hpp"
#include "../include/CPU.hpp"
#include "../include/ROB.hpp"
#include <cstdint>
#include <stdexcept>

bool LSQ::isEmpty() const { return tail == head; }

bool LSQ::isFull() const { return ((tail + 1) & 0x3F) == head; }

bool LSQ::isActive(uint8_t index) const {
  if (head == tail)
    return false;
  return ((index - head + LSQ_CAP) & 0x3F) < ((tail - head + LSQ_CAP) & 0x3F);
}

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
  for (int k = 1; k <= LSQ_CAP; ++k) {
    uint8_t i = (index + k) & 0x3F;
    if (i == index || !isActive(i))
      continue;
    if (((i - index) & 0x3F) >= ((tail - index) & 0x3F))
      continue;
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
    for (int k = 1; k <= LSQ_CAP; ++k) {
      uint8_t i = (index + k) & 0x3F;
      if (i == index || !isActive(i))
        continue;
      if (((i - index) & 0x3F) >= ((tail - index) & 0x3F))
        continue;
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
    bool found = false;
    for (int k = 0; k < LSQ_CAP; k++) {
      uint8_t i = (index - k + LSQ_CAP) & 0x3F;
      if (!isActive(i) || found)
        continue;
      if (((index - i) & 0x3F) > ((index - head) & 0x3F))
        continue;
      if (!LSQqueue[i].isLoad) {
        if (LSQqueue[i].isAddressReady && LSQqueue[i].address == address) {
          plan.writes[plan.count++] = {static_cast<uint8_t>(index),
                                       LSQqueue[i].value, LSQqueue[i].robTag,
                                       unknownBiggestStore == index &&
                                           LSQqueue[i].valueState ==
                                               ValueState::READY};
          found = true;
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
  bool blocked = false;
  for (int k = 0; k < LSQ_CAP; k++) {
    uint8_t cur = (head + k) & 0x3F;
    if (!isActive(cur) || blocked)
      continue;
    if (!LSQqueue[cur].isLoad) {
      if (!LSQqueue[cur].isAddressReady) {
        hasUnknownStore = true;
      }
    } else if (LSQqueue[cur].isAddressReady &&
               LSQqueue[cur].valueState == ValueState::NOTREADY) {
      // Load with known address, value not yet dispatched to memory
      if (hasUnknownStore) {
        blocked = true;
        continue;
      }
      if (LSQqueue[cur].valueState == ValueState::NOTREADY) {
        bool hasPendingSameAddrStore = false;
        for (int j = 0; j < LSQ_CAP; ++j) {
          uint8_t i = (head + j) & 0x3F;
          if (!isActive(i) || hasPendingSameAddrStore)
            continue;
          if (((i - head) & 0x3F) >= ((cur - head) & 0x3F))
            continue;
          if (!LSQqueue[i].isLoad && LSQqueue[i].isAddressReady &&
              LSQqueue[i].address == LSQqueue[cur].address) {
            hasPendingSameAddrStore = true;
          }
        }
        if (hasPendingSameAddrStore) {
          blocked = true;
          continue;
        }
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
  bool found = false;
  int detectedIndex = 0xFFFFFFFF;
  for (int k = 0; k < LSQ_CAP; ++k) {
    uint8_t cur = (head + k) & 0x3F;
    if (!isActive(cur) || found) continue;
    if (LSQqueue[cur].isLoad) {
      if (LSQqueue[cur].isAddressReady &&
          LSQqueue[cur].valueState == ValueState::READY &&
          !LSQqueue[cur].isCDBBroadcast) {
        detectedIndex = cur;
        found = true;
      }
    }
  }
  return detectedIndex;
}

bool LSQ::isReadyToCommit(int index) const {
  if (LSQqueue[index].isAddressReady &&
      LSQqueue[index].valueState == ValueState::READY) {
    return true;
  }
  return false;
}

MemDispatchDecision LSQ::selectMemRequest(const ROB &rob, const DMEM &dmem,
                                          const SquashInfo &squash) const {
  MemDispatchDecision memDecision{};
  if (!dmem.isBusy() && !isEmpty() && !isHeadLoad()) {
    auto storeTag = headRobTag();
    bool committed = rob.isEmpty() || ROB::isOlder(storeTag, rob.getHead());
    bool atHeadReady = !committed && headRobTag() == rob.getHead() &&
                       rob.isCommitReadyAt(rob.getIndexByTag(headRobTag()));
    if (committed || atHeadReady) {
      MemRequest newRequest{};
      newRequest.address = getAddress(getHead());
      newRequest.value = getValue(getHead());
      newRequest.isSigned = !getIsUnsigned(getHead());
      newRequest.n_bytes = getNBytes(getHead());
      newRequest.op = Operation::Store;
      newRequest.robTag = storeTag;
      newRequest.lsqIndex = head;
      memDecision.valid = true;
      memDecision.request = newRequest;
    }
  }
  if (memDecision.valid)
    return memDecision;
  auto loadIndex = LoadDetect();
  if (loadIndex != 0xFFFFFFFF && !dmem.isBusy()) {
    MemRequest newRequest{};
    newRequest.address = getAddress(loadIndex);
    newRequest.isSigned = !getIsUnsigned(loadIndex);
    newRequest.n_bytes = getNBytes(loadIndex);
    newRequest.op = Operation::Load;
    newRequest.robTag = getRobTag(loadIndex);
    newRequest.lsqIndex = loadIndex;
    if (!squash.needSquash ||
        (squash.needSquash &&
         ROB::isOlder(newRequest.robTag, squash.SquashTag))) {
      memDecision.valid = true;
      memDecision.request = newRequest;
    }
  }
  return memDecision;
}

void LSQ::tick(const LSQInput &input, systemState &CPUstate) {
  // issue apply: push the pre-built load/store entries
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
    }
  }
  // dispatch possible memRequest (store first, then load); decision was
  // pre-computed in the comb phase (CPU::read) and fed via both Inputs
  const auto &decision = input.decision;
  bool storeDispatched = false;
  if (decision.valid) {
    if (decision.request.op == Operation::Store) {
      storeDispatched = true;
    } else {
      CPUstate.LSQModule.setValueState(decision.request.lsqIndex,
                                       ValueState::FETCHING);
    }
  }
  uint8_t cur = getHead();
  bool retireLoad = !input.squashDetect.needSquash && cur != getTail() &&
                    isHeadLoad() &&
                    (input.ROBModule.isEmpty() ||
                     ROB::isOlder(headRobTag(), input.ROBModule.getHead()));
  (storeDispatched || retireLoad) ? CPUstate.LSQModule.pop() : void();

  // LSQ receive the value from DMEM
  if (input.loadResp.valid)
    CPUstate.LSQModule.writeValue(input.loadResp.value,
                                  input.loadResp.lsqIndex);

  // LSQ receive the value from AGU
  if (!input.AGUModule.isEmpty()) {
    auto aguRobTag = input.AGUModule.headRobTag();
    auto aguLsqIndex = input.AGUModule.headlsqIndex();
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(aguRobTag, input.squashDetect.SquashTag))) {
      auto value = input.AGUModule.headValue();
      auto plan = planAddressForward(aguLsqIndex, value);
      CPUstate.LSQModule.writeAddress(static_cast<uint32_t>(value),
                                      aguLsqIndex);
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
