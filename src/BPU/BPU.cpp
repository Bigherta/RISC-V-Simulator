#include "../include/BPU.hpp"
#include "../include/CPU.hpp"
#include <cstdint>
#include <cstring>

FetchDecision FetchDecision::build(const BPU &bp, uint32_t pc,
                                   const SquashInfo &squash, bool haltFetched,
                                   bool fqFull, bool imemReqFull) {
  FetchDecision fdec{};
  if (!squash.needSquash && !haltFetched && !fqFull && !imemReqFull) {
    auto prediction = bp.predict(pc);
    fdec.valid = true;
    fdec.pc = pc;
    fdec.predictedPC = prediction.taken ? prediction.predictPC : pc + 4;
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
    fdec.ckptId = bp.getNextCkptId();
  }
  return fdec;
}

PredictInfo BPU::predict(int32_t pc) const {
  auto local_index = (pc >> 2) & (LHT_CAP - 1);
  auto global_index = ((pc >> 2) ^ GHR) & (PC_Direct_CAP - 1);
  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
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
    // RET (JALR x0,0(x1)): predict the return address from the RAS stack top
    // (LIFO, ring index) instead of a single BTB target -- the BTB can only
    // store one return point when a function is called from multiple sites.
    if (BTB[BTB_index].isRet && RAS_top > 0)
      predictPC =
          static_cast<int32_t>(RAS[(RAS_top - 1) & (RAS_CAP - 1)].retPC);
    else
      predictPC = BTB[BTB_index].target;
  }
  PredictInfo out{taken, predictPC};
  out.btbHit = hit;
  out.unconditional = hit && BTB[BTB_index].unconditional;
  out.isCall = hit && BTB[BTB_index].isCall;
  out.isRet = hit && BTB[BTB_index].isRet;
  return out;
}
void BPU::update(int32_t pc, bool taken, int32_t target, uint16_t ghr) {
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

  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  if (taken) {
    BTB[BTB_index].actualPC = pc;
    BTB[BTB_index].target = target;
    BTB[BTB_index].valid = true;
    BTB[BTB_index].unconditional = false;
    BTB[BTB_index].isCall = false;
    BTB[BTB_index].isRet = false;
  }
}
void BPU::updateJump(int32_t pc, int32_t target, bool isCall, bool isRet) {
  auto BTB_index = (pc >> 2) & (BTB_CAP - 1);
  BTB[BTB_index].actualPC = pc;
  BTB[BTB_index].target = target;
  BTB[BTB_index].valid = true;
  BTB[BTB_index].unconditional = true;
  BTB[BTB_index].isCall = isCall;
  BTB[BTB_index].isRet = isRet;
}
void BPU::shiftGHR(bool taken) {
  GHR = ((GHR << 1) | (taken ? 1 : 0)) & HISTORY_MASK;
}
BPUSnapshot BPU::snapshotCheckPoint() const {
  // SARAS: the checkpoint keeps GHR, AlignQueue head+tail, and RAS_top.
  // With RASEntry{retPC,times}, height != call/ret depth, so RAS_top is
  // checkpointed directly instead of reconstructing from STC.
  BPUSnapshot s;
  s.GHR_snapshot = GHR;
  s.alignHead = alignHead;
  s.alignTail = alignTail;
  s.RAS_top = RAS_top;
  return s;
}

void BPU::recoverCheckPoint(const BPUSnapshot &ckpt) {
  GHR = ckpt.GHR_snapshot;
  alignHead = ckpt.alignHead;
  alignTail = ckpt.alignTail;
  RAS_top = ckpt.RAS_top;
}

void BPU::tick(const BPUpdateInput &input, systemState &CPUstate) {
  Cand bru, cdb;
  // collect candidate 1: BRU (B-type branch completed)
  if (!input.BRUModule.isEmpty()) {
    uint8_t brRobTag = input.BRUModule.headRobTag();
    int pcResult = input.BRUModule.headPCResult();
    int pcFrom = input.BRUModule.headPCFrom();
    {
      ++CPUstate.BPUModule.branchTotal;
      bool correct = pcResult == input.ROBModule.getPredictedPC(
                                     input.ROBModule.getIndexByTag(brRobTag));
      if (correct)
        ++CPUstate.BPUModule.branchCorrect;
      if (!input.squashDetect.needSquash ||
          (input.squashDetect.needSquash &&
           ROB::isOlder(brRobTag, input.squashDetect.SquashTag))) {
        bru.valid = true;
        bru.pc = pcFrom;
        bru.taken = pcResult != pcFrom + 4;
        bru.target = pcResult;
        bru.ghr = bpCkpt[input.ROBModule.getCkptId(
                                   input.ROBModule.getIndexByTag(brRobTag))]
                      .GHR_snapshot;
      }
    }
  }
  // collect candidate 2: CDB (JAL/JALR control transfer completed)
  auto cdbOut = input.cdbOut;
  if (cdbOut.valid && cdbOut.result.isControl && !input.ROBModule.isEmpty() &&
      !ROB::isOlder(cdbOut.result.robTag, input.ROBModule.getHead())) {
    auto robIdx = input.ROBModule.getIndexByTag(cdbOut.result.robTag);
    const auto pc = static_cast<uint32_t>(cdbOut.result.value);
    if (!input.squashDetect.needSquash ||
        (input.squashDetect.needSquash &&
         ROB::isOlder(cdbOut.result.robTag, input.squashDetect.SquashTag))) {
      ++CPUstate.BPUModule.branchTotal;
      bool correct = pc == input.ROBModule.getPredictedPC(robIdx);
      if (correct)
        ++CPUstate.BPUModule.branchCorrect;
      cdb.valid = true;
      cdb.pc = input.ROBModule.getPC(robIdx);
      cdb.taken = true;
      cdb.target = static_cast<int32_t>(pc);
      cdb.ghr = bpCkpt[input.ROBModule.getCkptId(robIdx)].GHR_snapshot;
      cdb.cond = false;
      cdb.isCall = input.ROBModule.isCall(robIdx);
      cdb.isRet = input.ROBModule.isRet(robIdx);
    }
  }

  // direction-split: conditional branches train BHT/LHT/Gshare + BTB,
  // JAL/JALR train only the BTB (with unconditional + call/ret type), so the
  // direction tables are never polluted by always-taken jumps.
  auto apply = [&](const Cand &c) {
    if (c.cond)
      CPUstate.BPUModule.update(c.pc, c.taken, c.target, c.ghr);
    else
      CPUstate.BPUModule.updateJump(c.pc, c.target, c.isCall, c.isRet);
  };
  // fixed order: BRU candidate first, CDB candidate second
  if (bru.valid)
    apply(bru);
  if (cdb.valid)
    apply(cdb);
  // fetch decision consumption: GHR/RAS are advanced at the prediction beat
  // (same cycle as the request was issued), so the LIFO pairing stays exact
  // (the decision is a read()-side value -- order-independent). The local
  // checkpoint pool records the pre-prediction state of each fetched
  // instruction (ckpt_id allocated at the comb() edge via nextCkptId).
  const auto &fd = input.fetchDecision;
  if (fd.valid) {
    CPUstate.BPUModule.bpCkpt[fd.ckptId] = snapshotCheckPoint();
    if (fd.isCall) {
      uint32_t ra = static_cast<uint32_t>(fd.pc + 4);
      // dedup: consecutive same-return recursive calls collapse into one entry
      // with an incremented times counter. Read snapshot RAS_top/RAS, write live.
      if (RAS_top > 0 &&
          RAS[(RAS_top - 1) & (RAS_CAP - 1)].retPC == ra) {
        AlignEntry e;
        e.addr = RAS[(RAS_top - 1) & (RAS_CAP - 1)].retPC;
        e.index = (RAS_top - 1) & (RAS_CAP - 1);
        e.times = RAS[(RAS_top - 1) & (RAS_CAP - 1)].times;
        CPUstate.BPUModule.alignQueue[alignTail & (ALIGNQ_CAP - 1)] = e;
        CPUstate.BPUModule.alignTail++;
        CPUstate.BPUModule.RAS[(RAS_top - 1) & (RAS_CAP - 1)].times++;
      } else {
        CPUstate.BPUModule.RAS[RAS_top & (RAS_CAP - 1)].retPC = ra;
        CPUstate.BPUModule.RAS[RAS_top & (RAS_CAP - 1)].times = 1;
        CPUstate.BPUModule.RAS_top++;
      }
    } else if (fd.isRet && RAS_top > 0) {
      uint32_t topIdx = (RAS_top - 1) & (RAS_CAP - 1);
      AlignEntry e;
      e.addr = RAS[topIdx].retPC;
      e.index = static_cast<uint8_t>(topIdx);
      e.times = RAS[topIdx].times;
      CPUstate.BPUModule.alignQueue[alignTail & (ALIGNQ_CAP - 1)] = e;
      CPUstate.BPUModule.alignTail++;
      if (RAS[topIdx].times > 1)
        CPUstate.BPUModule.RAS[topIdx].times--;
      else
        CPUstate.BPUModule.RAS_top--;
    }
    if (fd.shift)
      CPUstate.BPUModule.shiftGHR(fd.shiftValue);
    CPUstate.BPUModule.nextCkptId = (fd.ckptId + 1) & (CKPT_CAP - 1);
  }
  // SARAS: advance AlignQueue head at the commit beat (ret commit +1).
  // With RASEntry{retPC,times}, the height no longer equals call/ret
  // count, so STC is removed; RAS_top is restored from the checkpoint.
  if (!input.squashDetect.needSquash && !input.ROBModule.isEmpty() &&
      input.ROBModule.isHeadCommitReady()) {
    auto hIdx = ROB::idx(input.ROBModule.getHead());
    if (input.ROBModule.headType() == ROBType::LINK &&
        input.ROBModule.isRet(hIdx)) {
      if (alignHead != alignTail)
        CPUstate.BPUModule.alignHead++;
    }
  }
  // flush: restore GHR / AlignQueue head+tail / RAS_top to the squash
  // point; use the AlignQueue to undo all wrong-path RAS mutations
  // (call-dedup times++ and ret times--/pop) by restoring
  // {addr,times} in reverse order.
  if (input.squashDetect.needSquash && input.squashDetect.SquashIndex >= 0) {
    const auto &ckpt = bpCkpt[input.squashDetect.CkptId];
    // undo wrong-path RAS mutations: walk back from the current tail to the
    // squash point's alignTail (writing back in reverse order). 
    uint8_t curTail = alignTail;
    uint8_t base = ckpt.alignTail;
    uint8_t dist = curTail - base;
    for (int k = 0; k < ALIGNQ_CAP; ++k) {
      if ((uint8_t)k >= dist)
        continue; // already before the squash point (not wrong path), skip
      uint8_t pos = curTail - 1 - (uint8_t)k;
      AlignEntry e = alignQueue[pos & (ALIGNQ_CAP - 1)];
      CPUstate.BPUModule.RAS[e.index & (RAS_CAP - 1)].retPC = e.addr;
      CPUstate.BPUModule.RAS[e.index & (RAS_CAP - 1)].times = e.times;
    }
    CPUstate.BPUModule.recoverCheckPoint(ckpt); // GHR + align head+tail + RAS_top
    CPUstate.BPUModule.nextCkptId =
        (input.squashDetect.CkptId + 1) & (CKPT_CAP - 1);
  }
}