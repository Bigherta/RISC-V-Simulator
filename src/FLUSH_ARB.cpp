#include "../include/FLUSH_ARB.hpp"

namespace dark {

namespace {
// sorted insert (ports FlushArbiter::receive, Arbiter.hpp:49-56) into local buffer
inline void insertReq(std::array<max_size_t, FLUSHARBITER_CAP> &v,
					  std::array<max_size_t, FLUSHARBITER_CAP> &t,
					  std::array<max_size_t, FLUSHARBITER_CAP> &p, int &count,
					  max_size_t tag, max_size_t pc) {
	int pos = 0;
	while (pos < count && t[pos] < tag) {
		++pos;
	}
	for (int i = count; i > pos; --i) {
		v[i] = v[i - 1];
		t[i] = t[i - 1];
		p[i] = p[i - 1];
	}
	v[pos] = 1;
	t[pos] = tag;
	p[pos] = pc;
	++count;
}
} // namespace

void FLUSH_ARB::work() {
	// local buffer: build the next-cycle request buffer first (Register
	// single-write: every slot Register is written at most once below)
	std::array<max_size_t, FLUSHARBITER_CAP> n_valid{};
	std::array<max_size_t, FLUSHARBITER_CAP> n_tag{};
	std::array<max_size_t, FLUSHARBITER_CAP> n_pc{};
	int n_count = 0;

	// 1. copy old buffer
	for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
		if (to_unsigned(req_valid[i])) {
			n_valid[n_count] = 1;
			n_tag[n_count] = to_unsigned(req_tag[i]);
			n_pc[n_count] = to_unsigned(req_pc[i]);
			++n_count;
		}
	}
	// 2. clear: drop requests with tag >= active squash tag (ports CPU.cpp:1040)
	//    (the active squash is this module's own old output)
	auto oldValid = to_unsigned(valid);
	auto oldTag = to_unsigned(tag);
	if (oldValid) {
		int w = 0;
		for (int i = 0; i < n_count; ++i) {
			if (n_tag[i] < oldTag) {
				n_valid[w] = n_valid[i];
				n_tag[w] = n_tag[i];
				n_pc[w] = n_pc[i];
				++w;
			}
		}
		n_count = w;
	}
	// 3. detect squashes (gated by existing squash, ports CPU.cpp:831-840/905-909)
	bool branchSquash = false;
	max_size_t branchTag = 0, branchPC = 0;
	if (static_cast<bool>(branch.valid) && static_cast<bool>(branch.rob_ok)
		&& (oldValid == 0 || to_unsigned(branch.tag) < oldTag)
		&& to_unsigned(branch.pc_result) != to_unsigned(branch.rob_predpc)) {
		branchSquash = true;
		branchTag = to_unsigned(branch.tag);
		branchPC = to_unsigned(branch.pc_result);
	}
	bool cdbSquash = false;
	max_size_t cdbTag = 0, cdbPC = 0;
	if (static_cast<bool>(cdb.valid) && static_cast<bool>(cdb.is_control)
		&& static_cast<bool>(cdb.rob_ok)
		&& (oldValid == 0 || to_unsigned(cdb.tag) < oldTag)
		&& to_unsigned(cdb.pc) != to_unsigned(cdb.rob_predpc)) {
		cdbSquash = true;
		cdbTag = to_unsigned(cdb.tag);
		cdbPC = to_unsigned(cdb.pc);
	}
	// 4. receive: both squashes -> keep only the smaller tag (ports CPU.cpp:913-916)
	if (branchSquash && cdbSquash) {
		if (branchTag < cdbTag) {
			insertReq(n_valid, n_tag, n_pc, n_count, branchTag, branchPC);
		} else {
			insertReq(n_valid, n_tag, n_pc, n_count, cdbTag, cdbPC);
		}
	} else if (branchSquash) {
		insertReq(n_valid, n_tag, n_pc, n_count, branchTag, branchPC);
	} else if (cdbSquash) {
		insertReq(n_valid, n_tag, n_pc, n_count, cdbTag, cdbPC);
	}
	// buffer holds at most 1 request: receive is at most 1/cycle and clear
	// empties everything (min tag >= squash tag); overflow is a template error
	if (n_count > FLUSHARBITER_CAP) {
		n_count = FLUSHARBITER_CAP;
	}
	// 5. commit buffer registers
	for (int i = 0; i < FLUSHARBITER_CAP; ++i) {
		if (i < n_count) {
			req_valid[i] <= 1;
			req_tag[i] <= n_tag[i];
			req_pc[i] <= n_pc[i];
		} else {
			req_valid[i] <= 0;
		}
	}
	// 6. output = arbitResult (min tag, ports Arbiter.hpp:58-70)
	if (n_count > 0) {
		valid <= 1;
		tag <= n_tag[0];
		pc <= n_pc[0];
	} else {
		valid <= 0;
	}
}

} // namespace dark
