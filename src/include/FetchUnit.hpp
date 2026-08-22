#pragma once
#include "common.hpp"
#include <cstdint>
#include <cstring>

struct systemState;

struct FetchUnitInput {
  SquashInfo squashDetect;
  FetchDecision fetchDecision;
  bool haltSignal = false;
};

class FetchUnit {
private:
  uint32_t programCounter = 0;
  bool haltFetched = false;
  void clear();
  void setPC(uint32_t pc);
  void setHaltFetched(bool v);

public:
  FetchUnit() { std::memset(this, 0, sizeof(*this)); }
  uint32_t getPC() const { return programCounter; }
  bool isHaltFetched() const { return haltFetched; }
  void tick(const FetchUnitInput &, systemState &);
};
