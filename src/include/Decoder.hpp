#pragma once
#ifndef DECODER_HPP
#define DECODER_HPP
#include <cstdint>
#include "common.hpp"

class Decoder {
public:
    static Instruct decode(int32_t raw_inst);
    static inline int32_t signExtend(int32_t raw_data, int len) {
        return (raw_data << (32 - len)) >> (32 - len);
    }
};
#endif // DECODER_HPP