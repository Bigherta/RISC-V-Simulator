#include "../include/ROB.hpp"
#include <cstdio>

static unsigned long long g_cyc = 0;

namespace dark {

namespace {
inline max_size_t packMeta(ROBType type, int dest, bool halt) {
	return (static_cast<max_size_t>(type) & 0x3u)
		| ((static_cast<max_size_t>(dest) & 0x1Fu) << 2)
		| (halt ? (1u << 7) : 0u);
}
inline max_size_t packCkpt(const BranchPredictorCkpt &c) {
	return (static_cast<max_size_t>(c.top) << 16) | (c.GHR & 0xFFFFu);
}
} // namespace

bool ROB::isEmpty() const {
	return head == tail;
}

bool ROB::isFull() const {
	return ((to_unsigned(tail) + 1) & 0x3Fu) == to_unsigned(head);
}

int ROB::getIndex(max_size_t tag) const {
	for (int i = static_cast<int>(to_unsigned(head));
		 i != static_cast<int>(to_unsigned(tail)); i = (i + 1) & 0x3F) {
		if (to_unsigned(slot_tag[i]) == tag) {
			return i;
		}
	}
	return -1;
}

bool ROB::getValueReady(int i) const {
	if (i < 0 || i >= ROB_CAP) {
		return false;
	}
	return static_cast<bool>(slot_value_ready[i]);
}

bool ROB::getCommitReady(int i) const {
	return static_cast<bool>(slot_commit_ready[i]);
}

max_size_t ROB::getValue(int i) const {
	return to_unsigned(slot_value[i]);
}

max_size_t ROB::getTag(int i) const {
	return to_unsigned(slot_tag[i]);
}

max_size_t ROB::getPredPC(int i) const {
	return to_unsigned(slot_predpc[i]);
}

max_size_t ROB::getPC(int i) const {
	return to_unsigned(slot_pc[i]);
}

max_size_t ROB::getDest(int i) const {
	return (to_unsigned(slot_meta[i]) >> 2) & 0x1Fu;
}

ROBType ROB::getType(int i) const {
	return static_cast<ROBType>(to_unsigned(slot_meta[i]) & 0x3u);
}

bool ROB::getHalt(int i) const {
	return ((to_unsigned(slot_meta[i]) >> 7) & 1u) != 0;
}

max_size_t ROB::getCkpt(int i) const {
	return to_unsigned(slot_ckpt[i]);
}

bool ROB::headCommitReady() const {
	return getCommitReady(static_cast<int>(to_unsigned(head)));
}

bool ROB::headValueReady() const {
	return getValueReady(static_cast<int>(to_unsigned(head)));
}

max_size_t ROB::headValue() const {
	return getValue(static_cast<int>(to_unsigned(head)));
}

max_size_t ROB::headDest() const {
	return getDest(static_cast<int>(to_unsigned(head)));
}

bool ROB::headHalt() const {
	return getHalt(static_cast<int>(to_unsigned(head)));
}

ROBType ROB::headType() const {
	return getType(static_cast<int>(to_unsigned(head)));
}

max_size_t ROB::headPC() const {
	return getPC(static_cast<int>(to_unsigned(head)));
}

max_size_t ROB::headPredPC() const {
	return getPredPC(static_cast<int>(to_unsigned(head)));
}

max_size_t ROB::headTag() const {
	return getTag(static_cast<int>(to_unsigned(head)));
}

void ROB::push(const ROBEntry &entry) {
	int i = static_cast<int>(to_unsigned(tail));
	max_size_t tag = to_unsigned(rob_tag) + 1;
	slot_meta[i] <= packMeta(entry.type, entry.dest, entry.halt);
	slot_pc[i] <= static_cast<max_size_t>(entry.pc);
	slot_predpc[i] <= entry.predictedPC;
	slot_tag[i] <= tag;
	slot_value[i] <= static_cast<max_size_t>(entry.value);
	slot_value_ready[i] <= (entry.state >= ROBState::ValueReady ? 1u : 0u);
	slot_commit_ready[i] <= (entry.state == ROBState::CommitReady ? 1u : 0u);
	slot_ckpt[i] <= packCkpt(entry.ras_ckpt);
	rob_tag <= tag;
	tail <= ((to_unsigned(tail) + 1) & 0x3Fu);
}

void ROB::pop() {
	head <= ((to_unsigned(head) + 1) & 0x3Fu);
}

void ROB::flush(max_size_t flushTag) {
	int first = -1;
	for (int i = static_cast<int>(to_unsigned(head));
		 i != static_cast<int>(to_unsigned(tail)); i = (i + 1) & 0x3F) {
		if (to_unsigned(slot_tag[i]) > flushTag) {
			first = i;
			break;
		}
	}
	if (first != -1) {
		tail <= static_cast<max_size_t>(first);
	}
}

void ROB::work() {
	++g_cyc;
	// 1. flush: truncate entries with tag > squash_tag (mutually exclusive with push)
	if (squash.valid) {
		flush(to_unsigned(squash.tag));
	}
	// 2. commit pop: head entry commit-ready and no squash; report halt/dest
	//    (halt flag written only on pop, kept otherwise - matches haltCommitted semantics)
	if (squash.valid == 0 && !isEmpty() && headCommitReady()) {
		int h = static_cast<int>(to_unsigned(head));
		commit_info.halt <= (getHalt(h) ? 1u : 0u);
		commit_info.dest <= getDest(h);
		fprintf(stderr, "C cyc=%llu h=%d dest=%llu pc=%llu val=%d cr=%d\n", g_cyc,
				h, getDest(h), getPC(h), static_cast<int>(getValue(h)),
				static_cast<int>(to_unsigned(slot_commit_ready[h])));
		pop();
	}
	// 3. exec: type-dependent write (matches CPU.cpp:615-620 / writeBack CDB branches)
	if (exec.valid) {
		int idx = getIndex(to_unsigned(exec.tag));
		if (idx >= 0) {
			if (exec.isAddress == 0 && exec.isControl == 0
				&& getType(idx) == ROBType::REGISTER) {
				// normal result: value + value_ready + commit_ready (plan C trio)
				slot_value[idx] <= to_unsigned(exec.value);
				slot_value_ready[idx] <= 1;
				slot_commit_ready[idx] <= 1;
			} else if (exec.isControl && getType(idx) == ROBType::LINK) {
				// JALR: value known at push; commit_ready only
				slot_commit_ready[idx] <= 1;
			} else if (exec.isAddress == 0 && exec.isControl == 0
				&& getType(idx) == ROBType::STORE) {
				// store: commit-ready via LSQ ready broadcast (CPU.cpp:732-747)
				slot_commit_ready[idx] <= 1;
			}
			// isAddress (load/store address): ROB untouched (LSQ path handles value)
			// other types (BRANCH/STORE): untouched
		}
	}
	// 4. bru: branch result -> BRANCH commit_ready (ports CPU.cpp:844-845;
	//    older-than-squash results still write while a squash is in flight)
	if (bru.valid && (squash.valid == 0
					  || to_unsigned(bru.tag) < to_unsigned(squash.tag))) {
		int idx = getIndex(to_unsigned(bru.tag));
		if (idx >= 0 && getType(idx) == ROBType::BRANCH) {
			slot_commit_ready[idx] <= 1;
		} else {
		}
	}
	// 5. query: reply in next cycle (visible next-next cycle)
	if (query.valid) {
		int idx = getIndex(to_unsigned(query.tag));
		if (idx >= 0) {
			query_reply.valid <= 1;
			query_reply.ready <= (getValueReady(idx) ? 1u : 0u);
			query_reply.value <= getValue(idx);
		} else {
			query_reply.valid <= 0;
		}
	} else {
		query_reply.valid <= 0;
	}
	// 5. push: allocate from RAT decision (full/squash blocks it)
	if (push_req.valid && squash.valid == 0 && !isFull()) {
		max_size_t meta = to_unsigned(push_req.meta);
		ROBEntry e;
		e.type = static_cast<ROBType>(meta & 0x3u);
		e.dest = static_cast<int>((meta >> 2) & 0x1Fu);
		e.halt = ((meta >> 7) & 1u) != 0;
		e.pc = static_cast<int32_t>(to_unsigned(push_req.pc));
		e.predictedPC = to_unsigned(push_req.predpc);
		e.value = static_cast<int32_t>(to_unsigned(push_req.value));
		e.state = static_cast<ROBState>(to_unsigned(push_req.value_ready)
									   | (to_unsigned(push_req.commit_ready) << 1));
		e.ras_ckpt.top = static_cast<int>(to_unsigned(push_req.ckpt) >> 16);
		e.ras_ckpt.GHR = static_cast<uint16_t>(to_unsigned(push_req.ckpt) & 0xFFFFu);
		push(e);
	}
}

} // namespace dark
