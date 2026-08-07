#include <cstdio>
#include "../include/REG.hpp"

namespace dark {

max_size_t REG::reg_ref(int i) const {
	if (i <= 0 || i >= REGISTER_CAP) {
		return 0;
	}
	return to_unsigned(regs[i]);
}

void REG::work() {
	static unsigned long long g_cyc3 = 0;
	if (g_cyc3 >= 690 && g_cyc3 <= 760) {
		fprintf(stderr, "G cyc=%llu head_cr=%d tag=%llu dest=%llu val=%d a0=%llu\n", g_cyc3,
				static_cast<int>(to_unsigned(head_commit_ready)),
				to_unsigned(head_tag), to_unsigned(head_dest),
				static_cast<int>(to_unsigned(head_value)), to_unsigned(regs[10]));
	}
	++g_cyc3;
	// commit writeback (same source/condition as ROB.pop, aligns CPU.cpp:945-951)
	if (head_commit_ready && head_halt == 0 && to_unsigned(head_dest) != 0) {
		auto type = static_cast<ROBType>(to_unsigned(head_type));
		if (type == ROBType::REGISTER || type == ROBType::LINK) {
			regs[to_unsigned(head_dest)] <= to_unsigned(head_value);
		}
	}
}

} // namespace dark
