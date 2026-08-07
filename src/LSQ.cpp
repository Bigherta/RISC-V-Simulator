#include "../include/LSQ.hpp"

namespace dark {

bool LSQ::isEmpty() const {
	return to_unsigned(head) == to_unsigned(tail);
}

bool LSQ::isFull() const {
	return ((to_unsigned(tail) + 1) & 0x3Fu) == to_unsigned(head);
}

int LSQ::getIndex(max_size_t tag) const {
	for (int cur = static_cast<int>(to_unsigned(head));
		 cur != static_cast<int>(to_unsigned(tail)); cur = (cur + 1) & 0x3F) {
		if (to_unsigned(slot_rob_tag[cur]) == tag) {
			return cur;
		}
	}
	return -1;
}

bool LSQ::isLoad(int i) const {
	return static_cast<bool>(slot_is_load[i]);
}

bool LSQ::isAddressReady(int i) const {
	return static_cast<bool>(slot_is_address_ready[i]);
}

max_size_t LSQ::getValue(int i) const {
	return to_unsigned(slot_value[i]);
}

max_size_t LSQ::getTag(int i) const {
	return to_unsigned(slot_rob_tag[i]);
}

max_size_t LSQ::getAddress(int i) const {
	return to_unsigned(slot_address[i]);
}

max_size_t LSQ::getNBytes(int i) const {
	return to_unsigned(slot_n_bytes[i]);
}

bool LSQ::getIsUnsigned(int i) const {
	return static_cast<bool>(slot_is_unsigned[i]);
}

max_size_t LSQ::getValueState(int i) const {
	return to_unsigned(slot_value_state[i]);
}

bool LSQ::getIsCDBBroadcast(int i) const {
	return static_cast<bool>(slot_is_cdb_broadcast[i]);
}

max_size_t LSQ::getKnownTag(int i) const {
	return to_unsigned(slot_known_tag[i]);
}

bool LSQ::isReadyToCommit(int i) const { // LSQ.cpp:219
	return static_cast<bool>(slot_is_address_ready[i])
		&& to_unsigned(slot_value_state[i]) == static_cast<max_size_t>(LSQValueState::READY);
}

bool LSQ::headStoreValid() const {
	return !isEmpty() && slot_is_load[to_unsigned(head)] == 0;
}

max_size_t LSQ::headStoreTag() const {
	return headStoreValid() ? to_unsigned(slot_rob_tag[to_unsigned(head)]) : 0;
}

bool LSQ::headLoadValid() const {
	return !isEmpty() && static_cast<bool>(slot_is_load[to_unsigned(head)]);
}

max_size_t LSQ::headLoadTag() const {
	return headLoadValid() ? to_unsigned(slot_rob_tag[to_unsigned(head)]) : 0;
}

int LSQ::loadDetect() const { // ports LSQ.cpp:166-202
	bool hasUnknownStore = false;
	if (isEmpty()) {
		return -1;
	}
	for (int cur = static_cast<int>(to_unsigned(head));
		 cur != static_cast<int>(to_unsigned(tail)); cur = (cur + 1) & 0x3F) {
		if (slot_is_load[cur] == 0) {
			if (!isAddressReady(cur)) {
				hasUnknownStore = true;
			}
		} else if (isAddressReady(cur)
				   && to_unsigned(slot_value_state[cur])
					   == static_cast<max_size_t>(LSQValueState::NOTREADY)) {
			if (hasUnknownStore) {
				break;
			}
			if (to_unsigned(slot_value_state[cur])
				== static_cast<max_size_t>(LSQValueState::NOTREADY)) {
				bool hasPendingSameAddrStore = false;
				for (int i = static_cast<int>(to_unsigned(head)); i != cur;
					 i = (i + 1) & 0x3F) {
					if (slot_is_load[i] == 0 && isAddressReady(i)
						&& to_unsigned(slot_address[i])
							== to_unsigned(slot_address[cur])) {
						hasPendingSameAddrStore = true;
						break;
					}
				}
				if (hasPendingSameAddrStore) {
					break;
				}
				return cur;
			}
		}
	}
	return -1;
}

int LSQ::cdbDetectIndex() const { // ports LSQ.cpp:204-217
	if (isEmpty()) {
		return -1;
	}
	for (int cur = static_cast<int>(to_unsigned(head));
		 cur != static_cast<int>(to_unsigned(tail)); cur = (cur + 1) & 0x3F) {
		if (static_cast<bool>(slot_is_load[cur]) && isAddressReady(cur)
			&& to_unsigned(slot_value_state[cur])
				== static_cast<max_size_t>(LSQValueState::READY)
			&& slot_is_cdb_broadcast[cur] == 0) {
			return cur;
		}
	}
	return -1;
}

int LSQ::lsqReadyIndex() const { // loads (CDBDetect) + stores (execute isReadyToCommit, CPU.cpp:732-747)
	if (isEmpty()) {
		return -1;
	}
	for (int cur = static_cast<int>(to_unsigned(head));
		 cur != static_cast<int>(to_unsigned(tail)); cur = (cur + 1) & 0x3F) {
		if (isAddressReady(cur)
			&& to_unsigned(slot_value_state[cur])
				== static_cast<max_size_t>(LSQValueState::READY)
			&& slot_is_cdb_broadcast[cur] == 0) {
			return cur;
		}
	}
	return -1;
}

LSQPlan LSQ::planDataForward(int index, max_size_t value) const { // LSQ.cpp:83-109
	LSQPlan plan;
	if (!isAddressReady(index)) {
		return plan;
	}
	if (index == ((static_cast<int>(to_unsigned(tail)) - 1) & 0x3F)) {
		return plan;
	}
	int unknownBiggestStore = index;
	int knownBiggestSameAddrStore = index;
	for (int i = (index + 1) & 0x3F; i != static_cast<int>(to_unsigned(tail));
		 i = (i + 1) & 0x3F) {
		if (static_cast<bool>(slot_is_load[i])
			&& to_unsigned(slot_address[i]) == to_unsigned(slot_address[index])) {
			plan.writes[plan.count++] = {
				i, static_cast<int>(value),
				std::max(to_unsigned(slot_known_tag[i]),
						 to_unsigned(slot_rob_tag[knownBiggestSameAddrStore])),
				unknownBiggestStore == index && knownBiggestSameAddrStore == index};
		} else if (slot_is_load[i] == 0) {
			if (!isAddressReady(i)) {
				unknownBiggestStore = i;
			} else if (to_unsigned(slot_address[i])
					   == to_unsigned(slot_address[index])) {
				knownBiggestSameAddrStore = i;
			}
		}
	}
	return plan;
}

LSQPlan LSQ::planAddressForward(int index, max_size_t address) const { // LSQ.cpp:111-155
	LSQPlan plan;
	if (slot_is_load[index] == 0) {
		if (index == ((static_cast<int>(to_unsigned(tail)) - 1) & 0x3F)) {
			return plan;
		}
		int unknownBiggestStore = index;
		int knownBiggestSameAddrStore = index;
		for (int i = (index + 1) & 0x3F; i != static_cast<int>(to_unsigned(tail));
			 i = (i + 1) & 0x3F) {
			if (static_cast<bool>(slot_is_load[i])
				&& to_unsigned(slot_address[i]) == address) {
				plan.writes[plan.count++] = {
					i, static_cast<int>(to_unsigned(slot_value[index])),
					std::max(to_unsigned(slot_known_tag[i]),
							 to_unsigned(slot_rob_tag[knownBiggestSameAddrStore])),
					unknownBiggestStore == index && knownBiggestSameAddrStore == index
						&& to_unsigned(slot_value_state[index])
							== static_cast<max_size_t>(LSQValueState::READY)};
			} else if (slot_is_load[i] == 0) {
				if (!isAddressReady(i)) {
					unknownBiggestStore = i;
				} else if (to_unsigned(slot_address[i]) == address) {
					knownBiggestSameAddrStore = i;
				}
			}
		}
	} else {
		int unknownBiggestStore = index;
		for (int i = index; i != ((static_cast<int>(to_unsigned(head)) - 1) & 0x3F);
			 i = (i + 63) & 0x3F) {
			if (slot_is_load[i] == 0) {
				if (isAddressReady(i) && to_unsigned(slot_address[i]) == address) {
					plan.writes[plan.count++] = {
						index, static_cast<int>(to_unsigned(slot_value[i])),
						to_unsigned(slot_rob_tag[i]),
						unknownBiggestStore == index
							&& to_unsigned(slot_value_state[i])
								== static_cast<max_size_t>(LSQValueState::READY)};
					break;
				} else if (!isAddressReady(i)) {
					unknownBiggestStore = i;
				}
			}
		}
	}
	return plan;
}

void LSQ::work() {
	// pending writes merged from all forward plans this cycle
	// (Register single-write: ports the silent overwrite of applyStoreToLoadForward)
	std::array<bool, LSQ_CAP> pend_known{};
	std::array<max_size_t, LSQ_CAP> pend_known_val{};
	std::array<bool, LSQ_CAP> pend_value{};
	std::array<max_size_t, LSQ_CAP> pend_value_val{};

	// 1. squash flush: truncate entries with tag > squash_tag (LSQ.cpp:36-46)
	if (squash.valid) {
		auto sqTag = to_unsigned(squash.tag);
		int first = -1;
		for (int cur = static_cast<int>(to_unsigned(head));
			 cur != static_cast<int>(to_unsigned(tail)); cur = (cur + 1) & 0x3F) {
			if (to_unsigned(slot_rob_tag[cur]) > sqTag) {
				first = cur;
				break;
			}
		}
		if (first != -1) {
			tail <= static_cast<max_size_t>(first);
		}
	}
	// 2. push (same-cycle with issue; CPU.cpp:263-270/335-341). The reference
	// assigns a fresh LSQEntry (all fields initialized); slot reuse must reset
	// every field, otherwise a recycled slot inherits the previous occupant's
	// address/ready flags and can dispatch with a stale address.
	if (push.valid && squash.valid == 0 && !isFull()) {
		int i = static_cast<int>(to_unsigned(tail));
		slot_is_load[i] <= to_unsigned(push.is_load);
		slot_rob_tag[i] <= to_unsigned(push.rob_tag);
		slot_n_bytes[i] <= to_unsigned(push.n_bytes);
		slot_is_unsigned[i] <= to_unsigned(push.is_unsigned);
		slot_address[i] <= 0;
		slot_value[i] <= 0;
		slot_known_tag[i] <= 0;
		slot_is_address_ready[i] <= 0;
		slot_value_state[i] <= static_cast<max_size_t>(LSQValueState::NOTREADY);
		slot_is_cdb_broadcast[i] <= 0;
		tail <= ((to_unsigned(tail) + 1) & 0x3Fu);
	}
	// 3. operand delivery (ports CPU.cpp:644-659; STORE_RS already squash-filtered)
	for (int k = 0; k < STORERS_CAP; ++k) {
		if (operand[k].valid == 0) {
			continue;
		}
		int idx = getIndex(to_unsigned(operand[k].tag));
		if (idx >= 0) {
			auto value = to_unsigned(operand[k].value);
			slot_value[idx] <= value;
			slot_value_state[idx] <= static_cast<max_size_t>(LSQValueState::READY);
			slot_is_cdb_broadcast[idx] <= 0;
			auto plan = planDataForward(idx, value);
			for (int w = 0; w < plan.count; ++w) {
				auto &wr = plan.writes[w];
				pend_known[wr.index] = true;
				pend_known_val[wr.index] = wr.knownTag;
				if (wr.setValue) {
					pend_value[wr.index] = true;
					pend_value_val[wr.index] = static_cast<max_size_t>(wr.value);
				}
			}
		}
	}
	// 4. CDB address write (ports CPU.cpp:882-889); later in cycle than delivery
	if (cdb.valid && cdb.is_address) {
		int idx = getIndex(to_unsigned(cdb.tag));
		if (idx >= 0) {
			auto addr = to_unsigned(cdb.value);
			slot_address[idx] <= addr;
			slot_is_address_ready[idx] <= 1;
			auto plan = planAddressForward(idx, addr);
			for (int w = 0; w < plan.count; ++w) {
				auto &wr = plan.writes[w];
				pend_known[wr.index] = true;
				pend_known_val[wr.index] = wr.knownTag;
				if (wr.setValue) {
					pend_value[wr.index] = true;
					pend_value_val[wr.index] = static_cast<max_size_t>(wr.value);
				}
			}
		}
	}
	// 5. apply merged forward writes (known_tag overwrite order preserved)
	for (int i = 0; i < LSQ_CAP; ++i) {
		if (pend_known[i]) {
			slot_known_tag[i] <= pend_known_val[i];
			if (pend_value[i]) {
				slot_value[i] <= pend_value_val[i];
				slot_value_state[i] <= static_cast<max_size_t>(LSQValueState::READY);
				slot_is_cdb_broadcast[i] <= 0;
			}
		}
	}
	// 6. store dispatch (ports CPU.cpp:693-715; memBusy blocks)
	bool store_dispatched = false;
	if (!isEmpty() && headStoreValid() && rob.store_dispatch_ok && mem_busy == 0) {
		int h = static_cast<int>(to_unsigned(head));
		mem_request.valid <= 1;
		mem_request.is_load <= 0;
		mem_request.address <= to_unsigned(slot_address[h]);
		mem_request.value <= to_unsigned(slot_value[h]);
		mem_request.n_bytes <= to_unsigned(slot_n_bytes[h]);
		mem_request.is_signed <= (slot_is_unsigned[h] ? 0u : 1u);
		mem_request.rob_tag <= to_unsigned(slot_rob_tag[h]);
		head <= ((to_unsigned(head) + 1) & 0x3Fu);
		store_dispatched = true;
	}
	// 7. load dispatch (ports CPU.cpp:716-731; memBusy blocks)
	if (store_dispatched == false) {
		int ld = loadDetect();
		if (ld != -1 && mem_busy == 0
			&& (squash.valid == 0
				|| to_unsigned(slot_rob_tag[ld]) < to_unsigned(squash.tag))) {
			mem_request.valid <= 1;
			mem_request.is_load <= 1;
			mem_request.address <= to_unsigned(slot_address[ld]);
			mem_request.value <= 0;
			mem_request.n_bytes <= to_unsigned(slot_n_bytes[ld]);
			mem_request.is_signed <= (slot_is_unsigned[ld] ? 0u : 1u);
			mem_request.rob_tag <= to_unsigned(slot_rob_tag[ld]);
			slot_value_state[ld] <= static_cast<max_size_t>(LSQValueState::FETCHING);
		} else {
			mem_request.valid <= 0;
		}
	}
	// 8. mem reply writeback (ports CPU.cpp:806-819: only loads are written back)
	if (mem_reply.valid && (squash.valid == 0
		|| to_unsigned(mem_reply.rob_tag) < to_unsigned(squash.tag))) {
		int idx = getIndex(to_unsigned(mem_reply.rob_tag));
		if (idx >= 0 && isLoad(idx)) {
			slot_value[idx] <= to_unsigned(mem_reply.value);
			slot_value_state[idx] <= static_cast<max_size_t>(LSQValueState::READY);
			slot_is_cdb_broadcast[idx] <= 0;
		}
	}
	// 9. CDB broadcast mark (ports CPU.cpp:877-881)
	if (cdb_lsq_granted) {
		auto idx = to_unsigned(cdb_lsq_index);
		if (idx < static_cast<max_size_t>(LSQ_CAP)) {
			slot_is_cdb_broadcast[idx] <= 1;
		}
	}
	// 10. head load retire (ports CPU.cpp:930-940; one per cycle)
	if (squash.valid == 0 && headLoadValid()
		&& static_cast<bool>(slot_is_cdb_broadcast[to_unsigned(head)])
		&& rob.head_load_absent) {
		head <= ((to_unsigned(head) + 1) & 0x3Fu);
	}
}

} // namespace dark
