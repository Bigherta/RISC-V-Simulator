#pragma once
#ifndef CDB_HPP
#define CDB_HPP
#include "common.hpp"

struct CDBOutput {
  ExecuteResult result;
  bool valid;
  bool aluGranted;
  bool lsqGranted;
};

class CDB {
public:
  CDB() = default;

  CDBOutput arbitrate(ExecuteResult aluResult, bool aluValid,
                      ExecuteResult lsqResult, bool lsqValid) const;
  
};

#endif // CDB_HPP