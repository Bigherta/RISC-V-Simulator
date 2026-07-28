#pragma once
#include <cstdint>
#ifndef REGISTER_HPP
#define REGISTER_HPP
class Register {
private:
  int32_t data;

public:
  void write(int32_t data_) { data = data_; }
  int32_t read() { return data; }
};
#endif // REGISTER_HPP