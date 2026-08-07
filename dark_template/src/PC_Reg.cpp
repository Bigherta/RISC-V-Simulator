#include "../include/PC_Reg.hpp"

namespace dark {

max_size_t PC_Reg::getPC() const {
	return to_unsigned(pc);
}

void PC_Reg::work() { // ports CPU.cpp:12-13/52 (squash reload / predicted advance)
	if (squash_valid) {
		pc <= to_unsigned(squash_pc);
	} else if (advance) {
		pc <= to_unsigned(next_pc);
	}
}

} // namespace dark
