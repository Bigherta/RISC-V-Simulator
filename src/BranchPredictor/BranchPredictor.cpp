#include "../include/BranchPredictor.hpp"
int32_t BranchPredictor::predict(int32_t pc){
    return pc + 4;
}