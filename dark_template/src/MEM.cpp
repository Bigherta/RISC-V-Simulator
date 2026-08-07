#include "../include/MEM.hpp"
#include <cstdio>
#include <iostream>

static unsigned long long g_cyc = 0;

namespace dark {

// ---------------------------------------------------------------------------
// IMEM
// ---------------------------------------------------------------------------

uint32_t IMEM::hex2uint32(int len, char hex[]) {
	uint32_t result = 0;
	for (int i = 0; i < len; i++) {
		result *= 0x10;
		uint32_t value =
			(hex[i] >= '0' && hex[i] <= '9') ? (hex[i] - '0') : (hex[i] - 'A' + 10);
		result += value;
	}
	return result;
}

void IMEM::load_ins() { // ports Memory::load_ins (Memory.cpp:6-27)
	while (!std::cin.eof()) {
		char sign = std::cin.get();
		if (sign == EOF) {
			return;
		}
		if (sign == '@') {
			char hex_address[9];
			std::cin >> hex_address;
			uint32_t cur_address = hex2uint32(8, hex_address);
			char hex_byte[3];
			while (std::cin >> hex_byte) {
				mem[cur_address++] = static_cast<uint8_t>(hex2uint32(2, hex_byte));
				while (std::cin.peek() == '\n' || std::cin.peek() == ' ') {
					std::cin.get();
				}
				if (std::cin.peek() == '@') {
					break;
				}
			}
		}
	}
}

void IMEM::load_from_stdin() {
	load_ins();
}

bool IMEM::getHaltFetched() const {
	return static_cast<bool>(halt_fetched);
}

bool IMEM::fetchingValid() const { // fetch gated on (ports CPU.cpp:16-30)
	return static_cast<bool>(squash_valid) == false
		&& static_cast<bool>(halt_fetched) == false
		&& static_cast<bool>(inq_full) == false;
}

max_size_t IMEM::computeFetchRaw() const { // raw of the current fetch (old state)
	if (fetchingValid() == false) {
		return 0;
	}
	auto p = to_unsigned(pc);
	if (p + 4 > MEM_SIZE) {
		return 0; // fetch past the backing (never legitimately reached)
	}
	return static_cast<max_size_t>(mem[p])
		| (static_cast<max_size_t>(mem[p + 1]) << 8)
		| (static_cast<max_size_t>(mem[p + 2]) << 16)
		| (static_cast<max_size_t>(mem[p + 3]) << 24);
}

max_size_t IMEM::computePredPC() const { // ports CPU.cpp:31-45
	auto p = to_unsigned(pc);
	auto raw = computeFetchRaw();
	auto opcode = raw & 0x7Fu;
	auto rd = (raw >> 7) & 0x1Fu;
	auto rs1 = (raw >> 15) & 0x1Fu;
	auto imm_i = (raw >> 20) & 0xFFFu;
	max_size_t predpc = static_cast<bool>(pred_taken) ? to_unsigned(pred_target)
													 : p + 4;
	// JALR link: RAS pop overrides the prediction (CPU.cpp:42-46)
	if (opcode == 0b1100111u && rd == 0 && (rs1 == 1 || rs1 == 5) && imm_i == 0
		&& static_cast<bool>(ras_empty) == false) {
		predpc = to_unsigned(ras_pop_value);
	}
	return predpc;
}

max_size_t IMEM::computeNextPC() const { // PC_Reg.next_pc (ports CPU.cpp:52)
	return computePredPC();
}

void IMEM::work() { // ports CPU::fetch (CPU.cpp:9-54)
	uint32_t raw = 0;
	auto p = to_unsigned(pc);
	// 1. squash: halt clear (PC reload handled by PC_Reg)
	if (squash_valid) {
		halt_fetched <= 0;
		push_valid <= 0;
		return;
	}
	// 2. read + halt detect (outside push gate, CPU.cpp:26-29)
	if (halt_fetched == 0 && p + 4 <= MEM_SIZE) {
		raw = static_cast<uint32_t>(mem[p])
			| (static_cast<uint32_t>(mem[p + 1]) << 8)
			| (static_cast<uint32_t>(mem[p + 2]) << 16)
			| (static_cast<uint32_t>(mem[p + 3]) << 24);
		if (raw == 0x0ff00513u) {
			halt_fetched <= 1;
		}
	}
	// 3. push (CPU.cpp:30-53): halt instruction still pushed this cycle (old halt)
	if (g_cyc >= 676 && g_cyc <= 712 && p >= 4088u && p <= 4124u) {
		fprintf(stderr, "F cyc=%llu pc=%u full=%d halt=%d\n", g_cyc, p,
				static_cast<int>(to_unsigned(inq_full)),
				static_cast<int>(to_unsigned(halt_fetched)));
	}
	if (halt_fetched == 0 && inq_full == 0) {
		push_valid <= 1;
		push_raw <= raw;
		push_pc <= p;
		push_predpc <= computePredPC();
		push_ckpt <= to_unsigned(pred_ckpt);
	} else {
		push_valid <= 0;
	}
}

// ---------------------------------------------------------------------------
// DMEM
// ---------------------------------------------------------------------------

int32_t DMEM::load_n_bytes(uint32_t addr, int n, bool isSigned) const {
	int32_t result = 0;
	for (int i = 0; i < n; i++) {
		if (addr + i >= MEM_SIZE) {
			break; // out-of-bounds access (reference throws; never legitimately reached)
		}
		auto byte_data = static_cast<uint32_t>(mem[addr + i]);
		result |= static_cast<int32_t>(byte_data << (i << 3));
		if (i == n - 1 && n < 4 && isSigned) {
			if (result & (1 << ((n << 3) - 1))) {
				auto mask = ~((1 << (n << 3)) - 1);
				result |= mask;
			}
		}
	}
	return result;
}

void DMEM::store_n_bytes(uint32_t addr, int value, int n) {
	if (addr <= 0x139Cu && addr + n > 0x139Cu) {
		fprintf(stderr, "S cyc=%llu jr@%u val=%d n=%d\n", g_cyc, addr, value, n);
	}
	for (int i = 0; i < n; i++) {
		if (addr + i >= MEM_SIZE) {
			break; // out-of-bounds access (reference throws; never legitimately reached)
		}
		mem[addr + i] = static_cast<uint8_t>(value >> (i << 3));
	}
}

bool DMEM::isBusyOrReq() const {
	return static_cast<bool>(busy) || static_cast<bool>(req.valid);
}

bool DMEM::isBusy() const {
	return static_cast<bool>(busy);
}

bool DMEM::isReady() const {
	return static_cast<bool>(reply_valid);
}

void DMEM::work() {
	++g_cyc;
	// 1. reply self-clear (MemPull semantics, Memory.cpp:36): keep 1 cycle for LSQ
	if (reply_valid) {
		reply_valid <= 0;
	}
	// 2. execution (Memory.cpp:60-76): decrement every cycle after accept
	if (busy) {
		auto rc = to_unsigned(remain_cycle) - 1;
		remain_cycle <= rc;
		if (rc == 0) {
			if (exec_is_load) {
				auto v = load_n_bytes(to_unsigned(exec_address),
									  static_cast<int>(to_unsigned(exec_n_bytes)),
									  static_cast<bool>(exec_is_signed));
				exec_value <= static_cast<max_size_t>(v);
				reply_value <= static_cast<max_size_t>(v);
			} else {
				store_n_bytes(to_unsigned(exec_address),
							  static_cast<int>(to_unsigned(exec_value)),
							  static_cast<int>(to_unsigned(exec_n_bytes)));
				reply_value <= to_unsigned(exec_value);
			}
			reply_rob_tag <= to_unsigned(exec_rob_tag);
			reply_valid <= 1;
			busy <= 0;
		}
	}
	// 3. accept (MemPush, Memory.cpp:29-34): never on a completion cycle (busy old)
	if (req.valid && busy == 0) {
		exec_is_load <= to_unsigned(req.is_load);
		exec_address <= to_unsigned(req.address);
		exec_value <= to_unsigned(req.value);
		exec_n_bytes <= to_unsigned(req.n_bytes);
		exec_is_signed <= to_unsigned(req.is_signed);
		exec_rob_tag <= to_unsigned(req.rob_tag);
		remain_cycle <= 3; // MemRequest default remainCycle
		busy <= 1;
	}
}

} // namespace dark
