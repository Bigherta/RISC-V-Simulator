#pragma once
#ifndef RS_HPP
#define RS_HPP
#include "common.hpp"
#include <cstdint>
struct ReservationStation {
  Op op;
  bool free;
  int32_t vj;
  int32_t vk;
  int qj = -1;
  int qk = -1;
  int ROB_dest;
  ReservationStation() : free(true) {}
  ReservationStation(Op type) : op(type), free(true) {}
};
#endif // RS_HPP