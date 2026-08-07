#include "../include/REG.hpp"

namespace dark {

max_size_t REG::reg_ref(int i) const {
	if (i <= 0 || i >= REGISTER_CAP) {
		return 0;
	}
	return to_unsigned(regs[i]);
}

void REG::work() {
	// commit writeback (same source/condition as ROB.pop, aligns CPU.cpp:945-951)
	if (head_commit_ready && head_halt == 0 && to_unsigned(head_dest) != 0) {
		auto type = static_cast<ROBType>(to_unsigned(head_type));
		if (type == ROBType::REGISTER || type == ROBType::LINK) {
			regs[to_unsigned(head_dest)] <= to_unsigned(head_value);
		}
	}
}

} // namespace dark
