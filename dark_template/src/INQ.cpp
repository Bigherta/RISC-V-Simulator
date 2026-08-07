#include "../include/INQ.hpp"
#include <cstdio>
#include <cstdlib>

static unsigned long long g_cyc = 0;

namespace dark {

bool INQ::isEmpty() const {
	return head == tail;
}

bool INQ::isFull() const {
	return ((to_unsigned(tail) + 1) & (INQ_CAP - 1)) == to_unsigned(head);
}

bool INQ::almostFull() const {
	// usable capacity is INQ_CAP-1 (one slot reserved by the ring full check);
	// keep FETCH_MARGIN slots free for pushes already in flight (fetch -> INQ
	// takes 2 cycles), so they never arrive at a full queue.
	constexpr unsigned FETCH_MARGIN = 3;
	unsigned occ = (to_unsigned(tail) - to_unsigned(head)) & (INQ_CAP - 1);
	return occ >= INQ_CAP - 1 - FETCH_MARGIN;
}

bool INQ::headValid() const {
	return !isEmpty();
}

bool INQ::headIsHalt() const {
	if (isEmpty()) {
		return false;
	}
	return static_cast<bool>(slot_isHalt[to_unsigned(head)]);
}

max_size_t INQ::headRaw() const {
	return to_unsigned(slot_raw[to_unsigned(head)]);
}

max_size_t INQ::headPC() const {
	return to_unsigned(slot_pc[to_unsigned(head)]);
}

max_size_t INQ::headPredPC() const {
	return to_unsigned(slot_predpc[to_unsigned(head)]);
}

max_size_t INQ::headCkpt() const {
	return to_unsigned(slot_ckpt[to_unsigned(head)]);
}

void INQ::push(max_size_t raw, max_size_t pc, max_size_t predpc, max_size_t ckpt) {
	int i = static_cast<int>(to_unsigned(tail));
	slot_raw[i] <= raw;
	slot_pc[i] <= pc;
	slot_predpc[i] <= predpc;
	slot_ckpt[i] <= ckpt;
	slot_isHalt[i] <= (raw == 0x0ff00513u ? 1u : 0u); // halt known at fetch
	tail <= ((to_unsigned(tail) + 1) & (INQ_CAP - 1));
}

void INQ::pop() {
	head <= ((to_unsigned(head) + 1) & (INQ_CAP - 1));
}

void INQ::clear() {
	head <= 0;
	tail <= 0;
}

void INQ::work() {
	++g_cyc;
	// squash: clear queue (aligns CPU.cpp:11 INQModule.clear())
	if (squash.valid) {
		clear();
		return;
	}
	{
		if (true) {
			fprintf(stderr, "I cyc=%llu head=%u tail=%u :", g_cyc,
					to_unsigned(head), to_unsigned(tail));
			for (int k = 0; k < INQ_CAP; ++k) {
				fprintf(stderr, " %u", to_unsigned(slot_pc[k]));
			}
			fprintf(stderr, "\n");
		}
	}
	// pop: Decoder consumed the head this cycle (same-cycle, no duplicate)
	if (pop_valid && !isEmpty()) {
		pop();
	}
	// push: fetch decision (M3 stub / M6 IMEM)
	if (push_req.valid) {
		if (isFull()) {
			fprintf(stderr, "FATAL cyc=%llu: INQ dropped fetch pc=%u\n",
					g_cyc, to_unsigned(push_req.pc));
			std::abort();
		}
		fprintf(stderr, "N cyc=%llu pushpc=%u\n", g_cyc, to_unsigned(push_req.pc));
		push(to_unsigned(push_req.raw), to_unsigned(push_req.pc),
			 to_unsigned(push_req.predpc), to_unsigned(push_req.ckpt));
	}
}

} // namespace dark
