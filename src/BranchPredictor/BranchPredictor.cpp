#include "../include/BranchPredictor.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

FetchDecision FetchDecision::build(const BranchPredictor &bp, uint32_t pc,
                                   const SquashInfo &squash, bool haltFetched,
                                   bool fqFull, bool imemReqFull) {
  FetchDecision fdec{};
  if (!squash.needSquash && !haltFetched && !fqFull && !imemReqFull) {
    auto prediction = bp.predict(pc);
    fdec.valid = true;
    fdec.pc = pc;
    fdec.predictedPC =
        prediction.taken ? prediction.predictPC : pc + 4;
    if (prediction.btbHit) {
      fdec.shift = true;
      if (prediction.isRet) {
        fdec.shiftValue = true;
      } else if (prediction.isCall) {
        fdec.isCall = true;
        fdec.shiftValue = true;
      } else if (prediction.unconditional) {
        fdec.shiftValue = true;
      } else {
        fdec.shiftValue = prediction.taken;
      }
    }
    fdec.ckpt = bp.snapshotCheckPoint();
  }
  return fdec;
}

void BranchPredictor::tick(const BPUpdateInput &input, systemState &CPUstate) {
  Cand bru, cdb;
  // collect candidate 1: BRU (B-type branch completed)
  if (!input.BRUModule.isEmpty()) {
    uint8_t brRobTag = input.BRUModule.headRobTag();
    int pcResult = input.BRUModule.headPCResult();
    int pcFrom = input.BRUModule.headPCFrom();
    {
      ++CPUstate.BPModule.branchTotal;
      bool correct = pcResult ==
                     input.ROBModule.getPredictedPC(
                         input.ROBModule.getIndexByTag(brRobTag));
      if (correct)
        ++CPUstate.BPModule.branchCorrect;
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(brRobTag, input.squashDetect.SquashTag))) {
        bru.valid = true;
        bru.pc = pcFrom;
        bru.taken = pcResult != pcFrom + 4;
        bru.target = pcResult;
        bru.ghr = input.ROBModule
                      .getRASCkpt(input.ROBModule.getIndexByTag(brRobTag))
                      .GHR_snapshot;
      }
    }
  }
  // collect candidate 2: CDB (JAL/JALR control transfer completed)
  auto cdbOut = input.cdbOut;
  if (cdbOut.valid && cdbOut.result.isControl && !input.ROBModule.isEmpty() &&
      !ROB::isOlder(cdbOut.result.robTag, input.ROBModule.headTag())) {
    auto robIdx = input.ROBModule.getIndexByTag(cdbOut.result.robTag);
    const auto pc = static_cast<uint32_t>(cdbOut.result.value);
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag))) {
      ++CPUstate.BPModule.branchTotal;
      bool correct = pc == input.ROBModule.getPredictedPC(robIdx);
      if (correct)
        ++CPUstate.BPModule.branchCorrect;
      cdb.valid = true;
      cdb.pc = input.ROBModule.getPC(robIdx);
      cdb.taken = true;
      cdb.target = static_cast<int32_t>(pc);
      cdb.ghr = input.ROBModule.getRASCkpt(robIdx).GHR_snapshot;
      cdb.cond = false;
      cdb.isCall = input.ROBModule.getIsCall(robIdx);
      cdb.isRet = input.ROBModule.getIsRet(robIdx);
    }
  }

  // direction-split: conditional branches train BHT/LHT/Gshare + BTB,
  // JAL/JALR train only the BTB (with unconditional + call/ret type), so the
  // direction tables are never polluted by always-taken jumps.
  auto apply = [&](const Cand &c) {
    if (c.cond)
      CPUstate.BPModule.update(c.pc, c.taken, c.target, c.ghr);
    else
      CPUstate.BPModule.updateJump(c.pc, c.target, c.isCall, c.isRet);
  };
  // fixed order: BRU candidate first, CDB candidate second
  if (bru.valid)
    apply(bru);
  if (cdb.valid)
    apply(cdb);
  // fetch decision consumption: GHR/RAS are advanced at the prediction beat
  // (same cycle as the request was issued), so the LIFO pairing stays exact
  // (the decision is a read()-side value -- order-independent).
  const auto &fd = input.fetchDecision;
  if (fd.valid) {
    if (fd.isCall && !RAS_full())
      CPUstate.BPModule.RAS_push(fd.pc + 4);
    if (fd.isRet && !RAS_empty())
      CPUstate.BPModule.pop();
    if (fd.shift)
      CPUstate.BPModule.shiftGHR(fd.shiftValue);
  }
  // flush: restore GHR/RAS from the squashed branch's checkpoint
  // (tables keep the guarded updates above; only GHR/RAS are rewound).
  if (input.squashDetect.needSquash &&
      input.squashDetect.SquashIndex >= 0)
    CPUstate.BPModule.recoverCheckPoint(
        input.ROBModule.getRASCkpt(input.squashDetect.SquashIndex));
}
PredictInfo BranchPredictor::predict(int32_t pc) const {
  auto local_index = (pc >> 2) & (LHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ GHR) & (PC_Direct_CAP - 1);
  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  auto selector_index = ((pc >> 2) ^ GHR) & (SELECTOR_CAP - 1);
  auto history = LHT[local_index];
  bool hit = BTB[BTB_index].valid && BTB[BTB_index].actualPC == pc;
  bool use_global = selector[selector_index] >= 2;
  bool taken;
  if (hit && BTB[BTB_index].unconditional)
    taken = true;
  else
    taken = hit && (use_global ? globalPHT[global_index] >= 2
                               : localPHT[history] >= 2);
  int32_t predictPC = pc + 4;
  if (taken && hit) {
    predictPC = BTB[BTB_index].target;
  }
  PredictInfo out{taken, predictPC};
  out.btbHit = hit;
  out.unconditional = hit && BTB[BTB_index].unconditional;
  out.isCall = hit && BTB[BTB_index].isCall;
  out.isRet = hit && BTB[BTB_index].isRet;
  return out;
}
void BranchPredictor::update(int32_t pc, bool taken, int32_t target,
                             uint16_t ghr) {
  auto local_index = (pc >> 2) & (LHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ ghr) & (PC_Direct_CAP - 1);
  auto selector_index = ((pc >> 2) ^ ghr) & (SELECTOR_CAP - 1);
  auto history = LHT[local_index];

  bool pred_local = localPHT[history] >= 2;
  bool pred_global = globalPHT[global_index] >= 2;

  localPHT[history] =
      taken
          ? (localPHT[history] < 3 ? localPHT[history] + 1 : localPHT[history])
          : (localPHT[history] > 0 ? localPHT[history] - 1 : localPHT[history]);
  LHT[local_index] =
      ((LHT[local_index] << 1) | (taken ? 1 : 0)) & (LOCAL_PHT_CAP - 1);
  globalPHT[global_index] =
      taken ? (globalPHT[global_index] < 3 ? globalPHT[global_index] + 1
                                           : globalPHT[global_index])
            : (globalPHT[global_index] > 0 ? globalPHT[global_index] - 1
                                           : globalPHT[global_index]);

  bool local_correct = pred_local == taken;
  bool global_correct = pred_global == taken;
  if (global_correct && !local_correct)
    selector[selector_index] = selector[selector_index] < 3
                                   ? selector[selector_index] + 1
                                   : selector[selector_index];
  else if (local_correct && !global_correct)
    selector[selector_index] = selector[selector_index] > 0
                                   ? selector[selector_index] - 1
                                   : selector[selector_index];

  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  if (taken) {
    BTB[BTB_index].actualPC = pc;
    BTB[BTB_index].target = target;
    BTB[BTB_index].valid = true;
    BTB[BTB_index].unconditional = false;
    BTB[BTB_index].isCall = false; 
    BTB[BTB_index].isRet = false;
  }
}
void BranchPredictor::updateJump(int32_t pc, int32_t target, bool isCall,
                                 bool isRet) {
  auto BTB_index = (pc >> 2) & (PC_Direct_CAP - 1);
  BTB[BTB_index].actualPC = pc;
  BTB[BTB_index].target = target;
  BTB[BTB_index].valid = true;
  BTB[BTB_index].unconditional = true;
  BTB[BTB_index].isCall = isCall;
  BTB[BTB_index].isRet = isRet;
}
void BranchPredictor::shiftGHR(bool taken) {
  GHR = ((GHR << 1) | (taken ? 1 : 0)) & HISTORY_MASK;
}
void BranchPredictor::RAS_push(uint32_t addr) {
  if (RAS_top == RAS_CAP)
    throw std::runtime_error("RAS overflow: push in full return-address stack");
  RAS[RAS_top++] = addr;
}

void BranchPredictor::pop() {
  if (RAS_top == 0)
    throw std::runtime_error(
        "RAS underflow: pop on empty return-address stack");
  --RAS_top;
}

bool BranchPredictor::RAS_empty() const { return RAS_top == 0; }
bool BranchPredictor::RAS_full() const { return RAS_top == RAS_CAP; }

BranchPredictorSnapshot BranchPredictor::snapshotCheckPoint() const {
  BranchPredictorSnapshot s;
  s.top_snapshot = RAS_top;
  s.GHR_snapshot = GHR;
  memcpy(s.RAS_snapshot, RAS, sizeof(RAS));
  return s;
}

void BranchPredictor::recoverCheckPoint(const BranchPredictorSnapshot &ckpt) {
  RAS_top = ckpt.top_snapshot;
  GHR = ckpt.GHR_snapshot;
  memcpy(RAS, ckpt.RAS_snapshot, sizeof(RAS));
}