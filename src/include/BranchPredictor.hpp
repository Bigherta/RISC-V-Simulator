#pragma once
#ifndef BRANCHPREDICTOR_HPP
#define BRANCHPREDICTOR_HPP
#include <cstdint>
#include "common.hpp"
class BranchPredictor{
    public:
    int32_t predict(int32_t pc);
};
#endif // BRANCHPREDICTOR_HPP