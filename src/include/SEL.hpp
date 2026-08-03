#pragma once
#ifndef SEL_HPP
#define SEL_HPP
#include "common.hpp"

class RATSEL {
public:
  struct PortPair {
    RATWritePort first;
    RATWritePort second;
  };

  static PortPair RATWrite(RATWritePort issuePort, RATWritePort commitPort) {
    if (issuePort.valid && commitPort.valid &&
        issuePort.reg == commitPort.reg) {
      return {issuePort, {}};
    }
    return {commitPort, issuePort};
  }
};
#endif
