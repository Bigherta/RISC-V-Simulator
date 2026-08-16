#include "../include/RS.hpp"
#include "../include/CPU.hpp"
int RSUnit::tryAllocInteger() const {
  for (int i = 0; i < INTEGERRS_CAP; i++)
    if (integerRS[i].free)
      return i;
  return -1;
}
int RSUnit::tryAllocLoad() const {
  for (int i = 0; i < LOADRS_CAP; i++)
    if (loadRS[i].free)
      return i;
  return -1;
}
int RSUnit::tryAllocStoreAddress() const {
  for (int i = 0; i < STORERS_CAP; i++)
    if (storeAddressRS[i].free)
      return i;
  return -1;
}
int RSUnit::tryAllocStoreValue() const {
  for (int i = 0; i < STORERS_CAP; i++)
    if (storeValueRS[i].free)
      return i;
  return -1;
}
int RSUnit::tryAllocBranch() const {
  for (int i = 0; i < BRANCHRS_CAP; i++)
    if (branchRS[i].free)
      return i;
  return -1;
}
void RSUnit::broadcast(const RSInput&input, systemState &CPUstate, int robIndex, int value) {
  int phy = input.ROBModule.getNewPhy(robIndex);
  if (phy < 0)
    return; // no dest register: nothing to broadcast
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (!integerRS[i].free &&
        integerRS[i].qj == phy) {
      CPUstate.RSModule.integerRS[i].vj = value;
      CPUstate.RSModule.integerRS[i].qj = -1;
    }
    if (!integerRS[i].free &&
        integerRS[i].qk == phy) {
      CPUstate.RSModule.integerRS[i].vk = value;
      CPUstate.RSModule.integerRS[i].qk = -1;
    }
  }
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (!loadRS[i].free && loadRS[i].qj == phy) {
      CPUstate.RSModule.loadRS[i].vj = value;
      CPUstate.RSModule.loadRS[i].qj = -1;
    }
    if (!loadRS[i].free && loadRS[i].qk == phy) {
      CPUstate.RSModule.loadRS[i].vk = value;
      CPUstate.RSModule.loadRS[i].qk = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!storeAddressRS[i].free &&
        storeAddressRS[i].qj == phy) {
      CPUstate.RSModule.storeAddressRS[i].vj = value;
      CPUstate.RSModule.storeAddressRS[i].qj = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!storeValueRS[i].free &&
        storeValueRS[i].qrs2 == phy) {
      CPUstate.RSModule.storeValueRS[i].vrs2 = value;
      CPUstate.RSModule.storeValueRS[i].qrs2 = -1;
    }
  }
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (!branchRS[i].free &&
        branchRS[i].qj == phy) {
      CPUstate.RSModule.branchRS[i].vj = value;
      CPUstate.RSModule.branchRS[i].qj = -1;
    }
    if (!branchRS[i].free &&
        branchRS[i].qk == phy) {
      CPUstate.RSModule.branchRS[i].vk = value;
      CPUstate.RSModule.branchRS[i].qk = -1;
    }
  }
}
void RSUnit::tick(const RSInput &input, systemState &CPUstate) {
  const auto &p = input.issuePacket;
  if (p.valid) {
    if (p.hasInteger) {
      CPUstate.RSModule.integerRS[p.integerSlot] = p.integerRS;
    } else if (p.hasLoad) {
      CPUstate.RSModule.loadRS[p.loadSlot] = p.loadRS;
    } else if (p.hasStore) {
      CPUstate.RSModule.storeAddressRS[p.storeAddrSlot] = p.storeAddrRS;
      CPUstate.RSModule.storeValueRS[p.storeValueSlot] = p.storeValueRS;
    } else if (p.hasBranch) {
      CPUstate.RSModule.branchRS[p.branchSlot] = p.branchRS;
    }
  }
  if (input.dispatchBus.alu.valid) {
    int idx = input.dispatchBus.alu.rsIndex;
    CPUstate.RSModule.integerRS[idx].free = true;
    CPUstate.RSModule.integerRS[idx].qj = -1;
    CPUstate.RSModule.integerRS[idx].qk = -1;
  }
  if (input.dispatchBus.agu.valid) {
    int idx = input.dispatchBus.agu.rsIndex;
    if (input.dispatchBus.agu.rsType == RSType::Load) {
      CPUstate.RSModule.loadRS[idx].free = true;
      CPUstate.RSModule.loadRS[idx].qj = -1;
      CPUstate.RSModule.loadRS[idx].qk = -1;
    } else {
      CPUstate.RSModule.storeAddressRS[idx].free = true;
      CPUstate.RSModule.storeAddressRS[idx].qj = -1;
    }
  }
  if (input.dispatchBus.bru.valid) {
    int idx = input.dispatchBus.bru.rsIndex;
    CPUstate.RSModule.branchRS[idx].free = true;
    CPUstate.RSModule.branchRS[idx].qj = -1;
    CPUstate.RSModule.branchRS[idx].qk = -1;
  }

  if (input.cdbBus.broadcastValid) {
    broadcast(input, CPUstate, input.cdbBus.robIndex,
              input.cdbBus.broadcastValue);
  }

  if (input.squashDetect.needSquash) {
    auto sq = input.squashDetect.SquashSeq;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!integerRS[i].free &&
          input.ROBModule.getSeq(integerRS[i].robIndex) > sq) {
        CPUstate.RSModule.integerRS[i].free = true;
        CPUstate.RSModule.integerRS[i].qj = -1;
        CPUstate.RSModule.integerRS[i].qk = -1;
      }
    }
    for (int i = 0; i < LOADRS_CAP; i++) {
      if (!loadRS[i].free && input.ROBModule.getSeq(loadRS[i].robIndex) > sq) {
        CPUstate.RSModule.loadRS[i].free = true;
        CPUstate.RSModule.loadRS[i].qj = -1;
        CPUstate.RSModule.loadRS[i].qk = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!storeAddressRS[i].free &&
          input.ROBModule.getSeq(storeAddressRS[i].robIndex) > sq) {
        CPUstate.RSModule.storeAddressRS[i].free = true;
        CPUstate.RSModule.storeAddressRS[i].qj = -1;
      }
    }
    for (int i = 0; i < BRANCHRS_CAP; i++) {
      if (!branchRS[i].free &&
          input.ROBModule.getSeq(branchRS[i].robIndex) > sq) {
        CPUstate.RSModule.branchRS[i].free = true;
        CPUstate.RSModule.branchRS[i].qj = -1;
        CPUstate.RSModule.branchRS[i].qk = -1;
      }
    }
    for (int i = 0; i < STORERS_CAP; i++) {
      if (!storeValueRS[i].free &&
          input.ROBModule.getSeq(storeValueRS[i].robIndex) > sq) {
        CPUstate.RSModule.storeValueRS[i].free = true;
        CPUstate.RSModule.storeValueRS[i].qrs2 = -1;
      }
    }
  }
}
