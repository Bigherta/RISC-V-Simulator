#include "../include/CDB_Arb.hpp"

namespace dark {

namespace {

// sources with tag > squash tag are invalidated (CDBArbiter::arbitrate)
inline bool aluOk(const CDB_Arb &a) {
	if (static_cast<bool>(a.alu_valid) == false) {
		return false;
	}
	if (static_cast<bool>(a.squash_valid)
		&& to_unsigned(a.alu_tag) > to_unsigned(a.squash_tag)) {
		return false;
	}
	return true;
}

inline bool lsqOk(const CDB_Arb &a) {
	if (static_cast<bool>(a.lsq_valid) == false) {
		return false;
	}
	if (static_cast<bool>(a.squash_valid)
		&& to_unsigned(a.lsq_tag) > to_unsigned(a.squash_tag)) {
		return false;
	}
	return true;
}

} // namespace

bool CDB_Arb::resultValid() const {
	return aluOk(*this) || lsqOk(*this);
}

max_size_t CDB_Arb::resultTag() const {
	if (resultValid() == false) {
		return 0;
	}
	if (lsqOk(*this)
		&& (aluOk(*this) == false
			|| to_unsigned(lsq_tag) < to_unsigned(alu_tag))) {
		return to_unsigned(lsq_tag);
	}
	return to_unsigned(alu_tag);
}

max_size_t CDB_Arb::resultValue() const {
	if (resultValid() == false) {
		return 0;
	}
	if (lsqOk(*this)
		&& (aluOk(*this) == false
			|| to_unsigned(lsq_tag) < to_unsigned(alu_tag))) {
		return to_unsigned(lsq_value);
	}
	return to_unsigned(alu_value);
}

bool CDB_Arb::resultIsAddress() const {
	return aluOk(*this)
		&& (lsqOk(*this) == false
			|| to_unsigned(lsq_tag) >= to_unsigned(alu_tag))
		&& static_cast<bool>(alu_is_address);
}

bool CDB_Arb::resultIsControl() const {
	return aluOk(*this)
		&& (lsqOk(*this) == false
			|| to_unsigned(lsq_tag) >= to_unsigned(alu_tag))
		&& static_cast<bool>(alu_is_control);
}

bool CDB_Arb::aluGranted() const {
	return resultValid() && lsqOk(*this) == false
		|| (resultValid()
			&& lsqOk(*this)
			&& to_unsigned(lsq_tag) >= to_unsigned(alu_tag));
}

bool CDB_Arb::lsqGranted() const {
	return resultValid() && lsqOk(*this)
		&& (aluOk(*this) == false
			|| to_unsigned(lsq_tag) < to_unsigned(alu_tag));
}

max_size_t CDB_Arb::lsqIndex() const {
	return lsqGranted() ? to_unsigned(lsq_index) : 0;
}

void CDB_Arb::work() {}

} // namespace dark
