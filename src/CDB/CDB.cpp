#include "../include/CDB.hpp"

CDBOutput CDB::arbitrate(ExecuteResult aluResult, bool aluValid,
                          ExecuteResult lsqResult, bool lsqValid) const {
  CDBOutput out = {{0, 0, false}, false, false, false};

  if (!aluValid && !lsqValid)
    return out;

  if (aluValid && !lsqValid) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
    return out;
  }

  if (!aluValid && lsqValid) {
    out.result = lsqResult;
    out.valid = true;
    out.lsqGranted = true;
    return out;
  }

  if (aluResult.robTag <= lsqResult.robTag) {
    out.result = aluResult;
    out.valid = true;
    out.aluGranted = true;
  } else {
    out.result = lsqResult;
    out.valid = true;
    out.lsqGranted = true;
  }
  return out;
}