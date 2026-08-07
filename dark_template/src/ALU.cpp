#include "../include/ALU.hpp"

namespace dark {

int32_t ALU::ALUCalculate(int32_t op1, int32_t op2, Operation op) {
	if (isMemoryOp(op) || isControlOp(op)) {
		return op1 + op2;
	}
	switch (op) {
	case Operation::ADD:
	case Operation::AUIPC:
		return op1 + op2;
	case Operation::SUB:
		return op1 - op2;
	case Operation::XOR:
		return op1 ^ op2;
	case Operation::OR:
		return op1 | op2;
	case Operation::AND:
		return op1 & op2;
	case Operation::SL:
		return static_cast<int32_t>(static_cast<uint32_t>(op1) << (op2 & 0x1F));
	case Operation::SRL:
		return static_cast<int32_t>(static_cast<uint32_t>(op1) >> (op2 & 0x1F));
	case Operation::SRA:
		return op1 >> (op2 & 0x1F);
	case Operation::SLT:
		return op1 < op2 ? 1 : 0;
	case Operation::SLTU:
		return static_cast<uint32_t>(op1) < static_cast<uint32_t>(op2) ? 1 : 0;
	case Operation::LUI:
		return op2;
	default:
		return 0;
	}
}

bool ALU::isEmpty() const {
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i]) {
			return false;
		}
	}
	return true;
}

bool ALU::isFull() const {
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] == 0) {
			return false;
		}
	}
	return true;
}

bool ALU::push(ExecuteResult result) {
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] == 0) {
			valid[i] <= 1;
			value[i] <= result.value;
			tag[i] <= result.robTag;
			isAddress[i] <= result.isAddress;
			isControl[i] <= result.isControl;
			return true;
		}
	}
	return false;
}

void ALU::remove(max_size_t robTag) {
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && tag[i] == robTag) {
			valid[i] <= 0;
			return;
		}
	}
}

void ALU::flush(max_size_t flushTag) {
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && tag[i] > flushTag) {
			valid[i] <= 0;
		}
	}
}

max_size_t ALU::peekTag() const {
	int best = -1;
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && (best == -1 || tag[i] < tag[best])) {
			best = i;
		}
	}
	return best >= 0 ? to_unsigned(tag[best]) : 0;
}

max_size_t ALU::peekValue() const {
	int best = -1;
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && (best == -1 || tag[i] < tag[best])) {
			best = i;
		}
	}
	return best >= 0 ? to_unsigned(value[best]) : 0;
}

bool ALU::peekIsAddress() const {
	int best = -1;
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && (best == -1 || tag[i] < tag[best])) {
			best = i;
		}
	}
	return best >= 0 && static_cast<bool>(isAddress[best]);
}

bool ALU::peekIsControl() const {
	int best = -1;
	for (int i = 0; i < ALU_CAP; ++i) {
		if (valid[i] && (best == -1 || tag[i] < tag[best])) {
			best = i;
		}
	}
	return best >= 0 && static_cast<bool>(isControl[best]);
}

void ALU::work() {
	if (squash_valid) {
		flush(to_unsigned(squash_tag));
	}
	if (cdb_valid) {
		remove(to_unsigned(cdb_tag));
	}
	if (rs_valid && (squash_valid == 0 || to_unsigned(rs_tag) < to_unsigned(squash_tag))) {
		auto op = static_cast<Operation>(to_unsigned(rs_op));
		auto value = ALUCalculate(to_signed(rs_vj), to_signed(rs_vk), op);
		ExecuteResult r{value, static_cast<int>(to_unsigned(rs_tag)),
						isMemoryOp(op), isControlOp(op)};
		if (push(r)) {
			result_valid <= 1;
			result_value <= static_cast<max_size_t>(value);
			result_tag <= to_unsigned(rs_tag);
			result_isAddress <= r.isAddress;
			result_isControl <= r.isControl;
		} else {
			result_valid <= 0;
		}
	} else {
		result_valid <= 0;
	}
}

} // namespace dark
