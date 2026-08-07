#include "../include/EXEC_SEL.hpp"

namespace dark {

namespace {

// 0 = none, 1 = int, 2 = load, 3 = store; smallest rob_tag wins
inline int pick(const EXEC_SEL &sel) {
	bool iv = static_cast<bool>(sel.int_c.valid);
	bool lv = static_cast<bool>(sel.load_c.valid);
	bool sv = static_cast<bool>(sel.store_c.valid);
	if (!iv && !lv && !sv) {
		return 0;
	}
	int best = 0;
	max_size_t bestTag = 0;
	if (iv) {
		best = 1;
		bestTag = to_unsigned(sel.int_c.tag);
	}
	if (lv && (best == 0 || to_unsigned(sel.load_c.tag) < bestTag)) {
		best = 2;
		bestTag = to_unsigned(sel.load_c.tag);
	}
	if (sv && (best == 0 || to_unsigned(sel.store_c.tag) < bestTag)) {
		best = 3;
		bestTag = to_unsigned(sel.store_c.tag);
	}
	return best;
}

} // namespace

bool EXEC_SEL::execValid() const {
	return pick(*this) != 0 && static_cast<bool>(alu_full) == false;
}

max_size_t EXEC_SEL::execOp() const {
	switch (pick(*this)) {
	case 1:
		return execValid() ? to_unsigned(int_c.op) : 0;
	case 2:
		return execValid() ? to_unsigned(load_c.op) : 0;
	case 3:
		return execValid() ? to_unsigned(store_c.op) : 0;
	default:
		return 0;
	}
}

max_size_t EXEC_SEL::execVj() const {
	switch (pick(*this)) {
	case 1:
		return execValid() ? to_unsigned(int_c.vj) : 0;
	case 2:
		return execValid() ? to_unsigned(load_c.vj) : 0;
	case 3:
		return execValid() ? to_unsigned(store_c.vj) : 0;
	default:
		return 0;
	}
}

max_size_t EXEC_SEL::execVk() const {
	switch (pick(*this)) {
	case 1:
		return execValid() ? to_unsigned(int_c.vk) : 0;
	case 2:
		return execValid() ? to_unsigned(load_c.vk) : 0;
	case 3:
		return execValid() ? to_unsigned(store_c.vk) : 0;
	default:
		return 0;
	}
}

max_size_t EXEC_SEL::execTag() const {
	switch (pick(*this)) {
	case 1:
		return execValid() ? to_unsigned(int_c.tag) : 0;
	case 2:
		return execValid() ? to_unsigned(load_c.tag) : 0;
	case 3:
		return execValid() ? to_unsigned(store_c.tag) : 0;
	default:
		return 0;
	}
}

max_size_t EXEC_SEL::execSrc() const {
	auto p = pick(*this);
	return p == 0 || execValid() == false ? 0 : static_cast<max_size_t>(p - 1);
}

max_size_t EXEC_SEL::execIndex() const {
	switch (pick(*this)) {
	case 1:
		return execValid() ? to_unsigned(int_c.index) : 0;
	case 2:
		return execValid() ? to_unsigned(load_c.index) : 0;
	case 3:
		return execValid() ? to_unsigned(store_c.index) : 0;
	default:
		return 0;
	}
}

void EXEC_SEL::work() {}

} // namespace dark
