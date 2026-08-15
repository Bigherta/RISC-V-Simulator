#pragma once
#include "LSQ.hpp"
#include "PRF.hpp"
#include "ROB.hpp"
#include "RS.hpp"
#include "common.hpp"
struct systemState;
struct CDBInput {
  SquashInfo squashDetect;
  const ROB &ROBModule;
  const LSQ &LSQModule;
  const RSUnit &RSModule;
  const PRF &PRFModule;
  CDBOutput cdbArbiter;
  CDBInput(const ROB &rob, const LSQ &lsq, const RSUnit &rs, const PRF &prf)
      : ROBModule(rob), LSQModule(lsq), RSModule(rs), PRFModule(prf) {}
};
class CDB {
public:
  void CDBBroadcast(const CDBInput &, systemState &, int robIndex, int value);
  void tick(const CDBInput &, systemState &);
};