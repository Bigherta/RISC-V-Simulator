#pragma once
#ifndef BRANCHPREDICTOR_HPP
#define BRANCHPREDICTOR_HPP
#include <cstdint>
#include "common.hpp"
class BranchPredictor{
    private:
    uint8_t BHT[BHT_CAP] = {};
    BTBEntry BTB[BTB_CAP] = {};
    public:
    PredictInfo predict(int32_t pc);
    void update(int32_t pc, bool taken, int32_t target);
};
#endif // BRANCHPREDICTOR_HPP