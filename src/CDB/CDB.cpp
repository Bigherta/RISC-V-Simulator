#include "../include/CDB.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"

void CDB::CDBBroadcast(const CDBInput &input, systemState &CPUstate,
                       int robIndex, int value) {
  int phy = input.ROBModule.getNewPhy(robIndex);
  if (phy < 0)
    return; // no dest register: nothing to broadcast
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (!input.RSModule.integerRS[i].free &&
        input.RSModule.integerRS[i].qj == phy) {
      CPUstate.RSModule.integerRS[i].vj = value;
      CPUstate.RSModule.integerRS[i].qj = -1;
    }
    if (!input.RSModule.integerRS[i].free &&
        input.RSModule.integerRS[i].qk == phy) {
      CPUstate.RSModule.integerRS[i].vk = value;
      CPUstate.RSModule.integerRS[i].qk = -1;
    }
  }
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (!input.RSModule.loadRS[i].free &&
        input.RSModule.loadRS[i].qj == phy) {
      CPUstate.RSModule.loadRS[i].vj = value;
      CPUstate.RSModule.loadRS[i].qj = -1;
    }
    if (!input.RSModule.loadRS[i].free &&
        input.RSModule.loadRS[i].qk == phy) {
      CPUstate.RSModule.loadRS[i].vk = value;
      CPUstate.RSModule.loadRS[i].qk = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!input.RSModule.storeAddressRS[i].free &&
        input.RSModule.storeAddressRS[i].qj == phy) {
      CPUstate.RSModule.storeAddressRS[i].vj = value;
      CPUstate.RSModule.storeAddressRS[i].qj = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!input.RSModule.storeValueRS[i].free &&
        input.RSModule.storeValueRS[i].qrs2 == phy) {
      CPUstate.RSModule.storeValueRS[i].vrs2 = value;
      CPUstate.RSModule.storeValueRS[i].qrs2 = -1;
    }
  }
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (!input.RSModule.branchRS[i].free &&
        input.RSModule.branchRS[i].qj == phy) {
      CPUstate.RSModule.branchRS[i].vj = value;
      CPUstate.RSModule.branchRS[i].qj = -1;
    }
    if (!input.RSModule.branchRS[i].free &&
        input.RSModule.branchRS[i].qk == phy) {
      CPUstate.RSModule.branchRS[i].vk = value;
      CPUstate.RSModule.branchRS[i].qk = -1;
    }
  }
}

void CDB::tick(const CDBInput &input, systemState &CPUstate) {
  // 本 tick 只做总线侧动作：广播到 RS、通知 LSQ（setCDBBroadcast）。
  // ROB commit-ready 由 ROBModule.tick 监听 cdbArbiter 自理（含 JALR squash）；
  // PRF 写回由 PRFModule.tick 监听 cdbArbiter 自理。
  CDBOutput cdbOut = input.cdbArbiter;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        cdbOut.result.robSeq < input.squashDetect.SquashSeq) {
      auto robIndex = cdbOut.result.robIndex;
      auto robSeq = cdbOut.result.robSeq;
      auto isControl = cdbOut.result.isControl;
      if (!isControl) {
        auto value = cdbOut.result.value;
        CDBBroadcast(input, CPUstate, robIndex, value);
        if (cdbOut.lsqGranted) {
          auto lsqIndex = input.LSQModule.getIndexBySeq(robSeq);
          if (lsqIndex >= 0)
            CPUstate.LSQModule.setCDBBroadcast(lsqIndex);
        }
      } else if (isControl) {
        if (!input.ROBModule.isEmpty() && robSeq >= input.ROBModule.headSeq()) {
          const auto value =
              input.PRFModule.getValue(input.ROBModule.getNewPhy(robIndex));
          CDBBroadcast(input, CPUstate, robIndex, value);
        }
      }
    }
  }
}