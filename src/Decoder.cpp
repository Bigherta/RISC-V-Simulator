#include "../include/Decoder.hpp"

namespace dark {

Instruct Decoder::decode(int32_t raw_inst) {
	Instruct inst;
	uint32_t raw = static_cast<uint32_t>(raw_inst);
	auto opcode = raw & 0x7F;
	inst.opcode = static_cast<int>(opcode);
	inst.funct3 = static_cast<int>((raw >> 12) & 0x7);
	inst.rd = static_cast<int>((raw >> 7) & 0x1F);
	inst.rs1 = static_cast<int>((raw >> 15) & 0x1F);
	inst.rs2 = static_cast<int>((raw >> 20) & 0x1F);

	auto getImm = [&](RISC_V type) -> int32_t {
		switch (type) {
		case RISC_V::Istar:
			return static_cast<int32_t>((raw >> 20) & 0x1F);
		case RISC_V::I:
			return signExtend(static_cast<int32_t>((raw >> 20) & 0xFFF), 12);
		case RISC_V::S:
			return signExtend(
				static_cast<int32_t>(((raw >> 7) & 0x1F) | ((raw >> 25) << 5)), 12);
		case RISC_V::B:
			return signExtend(
				static_cast<int32_t>(((raw >> 31) << 12)
									 | (((raw >> 25) & 0x3F) << 5)
									 | ((inst.rd & 1) << 11)
									 | ((inst.rd >> 1) << 1)), 13);
		case RISC_V::J:
			return signExtend(
				static_cast<int32_t>(((raw >> 31) << 20)
									 | (((raw >> 21) & 0x3FF) << 1)
									 | (((raw << 11) >> 31) << 11)
									 | (((raw << 12) >> 24) << 12)), 21);
		case RISC_V::U:
			return static_cast<int32_t>(raw_inst & 0xFFFFF000);
		default:
			return 0;
		}
	};

	switch (opcode) {
	case 0b0110011u: { // R
		inst.type = RISC_V::R;
		inst.funct7 = static_cast<int>((raw >> 25) & 0x7F);
		break;
	}
	case 0b0010011u: { // I / Istar
		inst.funct7 = static_cast<int>((raw >> 25) & 0x7F);
		auto link_funct = (inst.funct3 << 7) | inst.funct7;
		if (link_funct == 0b0010000000 || link_funct == 0b1010000000
			|| link_funct == 0b1010100000) {
			inst.type = RISC_V::Istar;
		} else {
			inst.type = RISC_V::I;
		}
		inst.imm = getImm(inst.type);
		break;
	}
	case 0b0000011u:
	case 0b1100111u: { // load / JALR
		inst.type = RISC_V::I;
		inst.imm = getImm(RISC_V::I);
		break;
	}
	case 0b0100011u: { // S
		inst.type = RISC_V::S;
		inst.imm = getImm(RISC_V::S);
		break;
	}
	case 0b1100011u: { // B
		inst.type = RISC_V::B;
		inst.imm = getImm(RISC_V::B);
		break;
	}
	case 0b1101111u: { // J
		inst.type = RISC_V::J;
		inst.imm = getImm(RISC_V::J);
		break;
	}
	case 0b0010111u:
	case 0b0110111u: { // U
		inst.type = RISC_V::U;
		inst.imm = getImm(RISC_V::U);
		break;
	}
	default:
		inst.type = RISC_V::RV_INVALID;
		break;
	}
	return inst;
}

bool DEC::isEmpty() const {
	return head == tail;
}

bool DEC::isFull() const {
	return ((to_unsigned(tail) + 1) & (DECODER_CAP - 1)) == to_unsigned(head);
}

bool DEC::consumingValid() const {
	return !isFull() && static_cast<bool>(inq_valid);
}

bool DEC::headValid() const {
	return !isEmpty();
}

bool DEC::headIsHalt() const {
	if (isEmpty()) {
		return false;
	}
	return static_cast<bool>(slot_isHalt[to_unsigned(head)]);
}

max_size_t DEC::headRd() const {
	return to_unsigned(slot_rd[to_unsigned(head)]);
}

max_size_t DEC::headRs1() const {
	return to_unsigned(slot_rs1[to_unsigned(head)]);
}

max_size_t DEC::headRs2() const {
	return to_unsigned(slot_rs2[to_unsigned(head)]);
}

max_size_t DEC::headImm() const {
	return to_unsigned(slot_imm[to_unsigned(head)]);
}

max_size_t DEC::headType() const {
	return to_unsigned(slot_type[to_unsigned(head)]);
}

max_size_t DEC::headOpcode() const {
	return to_unsigned(slot_opcode[to_unsigned(head)]);
}

max_size_t DEC::headFunct3() const {
	return to_unsigned(slot_funct[to_unsigned(head)]) & 0x7u;
}

max_size_t DEC::headFunct7() const {
	return to_unsigned(slot_funct[to_unsigned(head)]) >> 3;
}

max_size_t DEC::headPC() const {
	return to_unsigned(slot_pc[to_unsigned(head)]);
}

max_size_t DEC::headPredPC() const {
	return to_unsigned(slot_predpc[to_unsigned(head)]);
}

max_size_t DEC::headCkpt() const {
	return to_unsigned(slot_ckpt[to_unsigned(head)]);
}

void DEC::pushEntry(Instruct inst, max_size_t pc, max_size_t predpc,
						max_size_t ckpt, bool isHalt) {
	int i = static_cast<int>(to_unsigned(tail));
	slot_imm[i] <= static_cast<max_size_t>(inst.imm);
	slot_rd[i] <= static_cast<max_size_t>(inst.rd);
	slot_rs1[i] <= static_cast<max_size_t>(inst.rs1);
	slot_rs2[i] <= static_cast<max_size_t>(inst.rs2);
	slot_type[i] <= static_cast<max_size_t>(inst.type);
	slot_opcode[i] <= static_cast<max_size_t>(inst.opcode);
	slot_funct[i] <= (static_cast<max_size_t>(inst.funct3)
					  | (static_cast<max_size_t>(inst.funct7) << 3));
	slot_pc[i] <= pc;
	slot_predpc[i] <= predpc;
	slot_ckpt[i] <= ckpt;
	slot_isHalt[i] <= (isHalt ? 1u : 0u);
	tail <= ((to_unsigned(tail) + 1) & (DECODER_CAP - 1));
}

void DEC::pop() {
	head <= ((to_unsigned(head) + 1) & (DECODER_CAP - 1));
}

void DEC::clear() {
	head <= 0;
	tail <= 0;
}

void DEC::work() {
	if (squash_valid) {
		clear();
		pop_request <= 0;
		return;
	}
	// pop: decoded head consumed by issue (RS-driven, aligns CPU.cpp:467-468)
	if (pop_dec_valid && !isEmpty()) {
		pop();
	}
	// pre-decode one entry from INQ head (INQ pops via consumingValid, same cycle)
	if (consumingValid()) {
		auto inst = Decoder::decode(to_signed(inq_raw));
		pushEntry(inst, to_unsigned(inq_pc), to_unsigned(inq_predpc),
				  to_unsigned(inq_ckpt), static_cast<bool>(inq_isHalt));
		pop_request <= 1; // INQ head consumed this cycle
	} else {
		pop_request <= 0;
	}
}

} // namespace dark
