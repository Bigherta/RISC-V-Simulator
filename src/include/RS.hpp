#pragma once
#ifndef RS_HPP
#define RS_HPP
#include "common.hpp"
#include <cstdint>
struct ReservationStation {
  Operation op;
  bool free;
  int32_t vj;
  int32_t vk;
  int32_t vrs2;
  int qj = -1;
  int qk = -1;
  int qrs2 = -1;
  int ROB_dest = ~0u >> 1;
  ReservationStation() : free(true) {}
  ReservationStation(Operation type) : op(type), free(true) {}
};

struct StoreMicroReservationStation {
  bool free;
  int32_t vrs2;
  int qrs2 = -1;
  int ROB_dest = ~0u >> 1;
  StoreMicroReservationStation() : free(true) {}
};
#endif // RS_HPP