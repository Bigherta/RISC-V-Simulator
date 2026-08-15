#include "../include/CDB.hpp"
#include "../include/CPU.hpp"
#include "../include/util.hpp"

void CDB::CDBBroadcast(const CDBInput &input, systemState &CPUstate,
                       int robIndex, int value) {
  int phy = input.ROBModule.getNewPhy(robIndex);
  if (phy < 0)
    return; // no dest register: nothing to broadcast
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    if (!input.IntegerRSModule.IntegerRS[i].free &&
        input.IntegerRSModule.IntegerRS[i].qj == phy) {
      CPUstate.IntegerRSModule.IntegerRS[i].vj = value;
      CPUstate.IntegerRSModule.IntegerRS[i].qj = -1;
    }
    if (!input.IntegerRSModule.IntegerRS[i].free &&
        input.IntegerRSModule.IntegerRS[i].qk == phy) {
      CPUstate.IntegerRSModule.IntegerRS[i].vk = value;
      CPUstate.IntegerRSModule.IntegerRS[i].qk = -1;
    }
  }
  for (int i = 0; i < LOADRS_CAP; i++) {
    if (!input.LoadRSModule.LoadRS[i].free &&
        input.LoadRSModule.LoadRS[i].qj == phy) {
      CPUstate.LoadRSModule.LoadRS[i].vj = value;
      CPUstate.LoadRSModule.LoadRS[i].qj = -1;
    }
    if (!input.LoadRSModule.LoadRS[i].free &&
        input.LoadRSModule.LoadRS[i].qk == phy) {
      CPUstate.LoadRSModule.LoadRS[i].vk = value;
      CPUstate.LoadRSModule.LoadRS[i].qk = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!input.StoreAddressRSModule.StoreAddressRS[i].free &&
        input.StoreAddressRSModule.StoreAddressRS[i].qj == phy) {
      CPUstate.StoreAddressRSModule.StoreAddressRS[i].vj = value;
      CPUstate.StoreAddressRSModule.StoreAddressRS[i].qj = -1;
    }
  }
  for (int i = 0; i < STORERS_CAP; i++) {
    if (!input.StoreValueRSModule.StoreValueRS[i].free &&
        input.StoreValueRSModule.StoreValueRS[i].qrs2 == phy) {
      CPUstate.StoreValueRSModule.StoreValueRS[i].vrs2 = value;
      CPUstate.StoreValueRSModule.StoreValueRS[i].qrs2 = -1;
    }
  }
  for (int i = 0; i < BRANCHRS_CAP; i++) {
    if (!input.BranchRSModule.BranchRS[i].free &&
        input.BranchRSModule.BranchRS[i].qj == phy) {
      CPUstate.BranchRSModule.BranchRS[i].vj = value;
      CPUstate.BranchRSModule.BranchRS[i].qj = -1;
    }
    if (!input.BranchRSModule.BranchRS[i].free &&
        input.BranchRSModule.BranchRS[i].qk == phy) {
      CPUstate.BranchRSModule.BranchRS[i].vk = value;
      CPUstate.BranchRSModule.BranchRS[i].qk = -1;
    }
  }
}

void CDB::tick(const CDBInput &input, systemState &CPUstate) {
  CDBOutput cdbOut = input.cdbArbiter;
  if (cdbOut.valid) {
    if (!input.squashDetect.needSquash ||
        cdbOut.result.robSeq < input.squashDetect.SquashSeq) {
      auto robIndex = cdbOut.result.robIndex;
      auto robSeq = cdbOut.result.robSeq;
      auto isControl = cdbOut.result.isControl;
      SquashInfo JumpSquash;
      if (!isControl) {
        auto value = cdbOut.result.value;
        CDBBroadcast(input, CPUstate, robIndex, value);
        if (!input.ROBModule.isEmpty() && robSeq >= input.ROBModule.headSeq()) {
          CPUstate.ROBModule.setROBCommitReady(robIndex);
        }
        if (cdbOut.lsqGranted) {
          auto lsqIndex = input.LSQModule.getIndexBySeq(robSeq);
          if (lsqIndex >= 0)
            CPUstate.LSQModule.setCDBBroadcast(lsqIndex);
        }
        int newPhy = input.ROBModule.getNewPhy(robIndex);
        if (newPhy >= 0) {
          CPUstate.PRFModule.write(newPhy, value);
          if (debug::enabled(debug::TOPIC_PRF)) {
            debug::print("PRF write P%d = %d (cdb)\n", newPhy, value);
            if (input.PRFModule.isReady(newPhy) &&
                input.PRFModule.getValue(newPhy) != value)
              debug::print("PRF mismatch P%d: rob=%d prf=%d\n", newPhy, value,
                           input.PRFModule.getValue(newPhy));
          }
        }
      } else if (isControl) {
        if (!input.ROBModule.isEmpty() && robSeq >= input.ROBModule.headSeq()) {
          const auto pc = static_cast<uint32_t>(cdbOut.result.value);
          const auto value =
              input.PRFModule.getValue(input.ROBModule.getNewPhy(robIndex));
          CDBBroadcast(input, CPUstate, robIndex, value);
          if (pc != input.ROBModule.getPredictedPC(robIndex)) {
            if (debug::enabled(debug::TOPIC_BRANCH))
              debug::print("squash seq=%llu pc=%u (jalr)\n",
                           static_cast<unsigned long long>(robSeq), pc);
            JumpSquash.needSquash = true;
            JumpSquash.SquashPC = pc;
            JumpSquash.SquashIndex = robIndex;
            JumpSquash.SquashSeq = robSeq;
          }
          CPUstate.ROBModule.setROBCommitReady(robIndex);
        }
      }
      if (JumpSquash.needSquash)
        CPUstate.flushArbiter.receive(JumpSquash);
    }
  }
}