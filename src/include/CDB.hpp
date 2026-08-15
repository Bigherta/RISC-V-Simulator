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
  const IntegerRS &IntegerRSModule;
  const LoadRS &LoadRSModule;
  const StoreAddressRS &StoreAddressRSModule;
  const StoreValueRS &StoreValueRSModule;
  const BranchRS &BranchRSModule;
  const PRF &PRFModule;
  CDBOutput cdbArbiter;
  CDBInput(const ROB &rob, const LSQ &lsq, const IntegerRS &irs,
           const LoadRS &lrs, const StoreAddressRS &sars,
           const StoreValueRS &svrs, const BranchRS &brs, const PRF &prf)
      : ROBModule(rob), LSQModule(lsq), IntegerRSModule(irs), LoadRSModule(lrs),
        StoreAddressRSModule(sars), StoreValueRSModule(svrs),
        BranchRSModule(brs), PRFModule(prf) {}
};
class CDB {
public:
  void CDBBroadcast(const CDBInput &, systemState &, int robIndex, int value);
  void tick(const CDBInput &, systemState &);
};