#include "../include/RAT.hpp"
#include "../include/ROB.hpp" // rob_ptr squash repair traversal (complete type)

static unsigned long long g_cyc = 0;

namespace dark {

namespace {
enum class IssType { Integer, Branch, Load, Store, Link, Halt, Invalid };

inline IssType classify(bool isHalt, max_size_t type, max_size_t opcode) {
	if (isHalt) {
		return IssType::Halt;
	}
	switch (static_cast<RISC_V>(type)) {
	case RISC_V::R:
	case RISC_V::Istar:
	case RISC_V::U:
		return IssType::Integer;
	case RISC_V::I:
		switch (opcode) {
		case 0b0010011u:
			return IssType::Integer;
		case 0b1100111u:
			return IssType::Link; // JALR
		case 0b0000011u:
			return IssType::Load;
		default:
			return IssType::Invalid;
		}
	case RISC_V::S:
		return IssType::Store;
	case RISC_V::B:
		return IssType::Branch;
	case RISC_V::J:
		return IssType::Link;
	default:
		return IssType::Invalid;
	}
}

inline IssType classifyOf(const RAT &rat) {
	return classify(static_cast<bool>(rat.dec.isHalt), to_unsigned(rat.dec.type),
					to_unsigned(rat.dec.opcode));
}

// 4-level operand resolution (ports CPU.cpp:77-114):
// REG direct value -> ROB ValueReady -> CDB bypass -> pending tag
struct Resolved {
	max_size_t value;
	max_size_t pending; // 0 = ready
};

inline Resolved resolveRs1(const RAT &rat) {
	auto reg = to_unsigned(rat.dec.rs1);
	auto tag = to_unsigned(rat.rat_table[reg]);
	if (tag == 0) {
		return {to_unsigned(rat.regfile.regs[reg]), 0};
	}
	if (static_cast<bool>(rat.robq.rs1_ready)) {
		return {to_unsigned(rat.robq.rs1_value), 0};
	}
	if (static_cast<bool>(rat.cdb.valid)
		&& !static_cast<bool>(rat.cdb.is_address)
		&& !static_cast<bool>(rat.cdb.is_control)
		&& to_unsigned(rat.cdb.tag) == tag) {
		return {to_unsigned(rat.cdb.value), 0};
	}
	return {0, tag};
}

inline Resolved resolveRs2(const RAT &rat) {
	auto reg = to_unsigned(rat.dec.rs2);
	auto tag = to_unsigned(rat.rat_table[reg]);
	if (tag == 0) {
		return {to_unsigned(rat.regfile.regs[reg]), 0};
	}
	if (static_cast<bool>(rat.robq.rs2_ready)) {
		return {to_unsigned(rat.robq.rs2_value), 0};
	}
	if (static_cast<bool>(rat.cdb.valid)
		&& !static_cast<bool>(rat.cdb.is_address)
		&& !static_cast<bool>(rat.cdb.is_control)
		&& to_unsigned(rat.cdb.tag) == tag) {
		return {to_unsigned(rat.cdb.value), 0};
	}
	return {0, tag};
}
} // namespace

bool RAT::computePushValid() const {
	if (squash.valid || dec.valid == 0) {
		return false;
	}
	switch (classifyOf(*this)) {
	case IssType::Integer:
	case IssType::Link:
		return stall.rs_int_full == 0 && rob.full == 0;
	case IssType::Branch:
		return stall.rs_branch_full == 0 && rob.full == 0;
	case IssType::Load:
		return stall.rs_load_full == 0 && rob.full == 0 && stall.lsq_full == 0;
	case IssType::Store:
		return stall.rs_store_full == 0 && stall.rs_micro_full == 0 && rob.full == 0
			&& stall.lsq_full == 0;
	case IssType::Halt:
		return rob.full == 0;
	default:
		return false;
	}
}

max_size_t RAT::computePushMeta() const {
	auto t = classifyOf(*this);
	ROBType rtype = ROBType::REGISTER;
	switch (t) {
	case IssType::Store:
		rtype = ROBType::STORE;
		break;
	case IssType::Branch:
		rtype = ROBType::BRANCH;
		break;
	case IssType::Link:
		rtype = ROBType::LINK;
		break;
	default:
		break;
	}
	auto dest = to_unsigned(dec.rd);
	return (static_cast<max_size_t>(rtype) & 0x3u)
		| ((dest & 0x1Fu) << 2)
		| (to_unsigned(dec.isHalt) ? (1u << 7) : 0u);
}

max_size_t RAT::computePushPC() const {
	auto t = classifyOf(*this);
	if (t == IssType::Integer || t == IssType::Link || t == IssType::Branch) {
		return to_unsigned(dec.pc);
	}
	return 0;
}

max_size_t RAT::computePushPredPC() const {
	auto t = classifyOf(*this);
	if (t == IssType::Integer || t == IssType::Link || t == IssType::Branch) {
		return to_unsigned(dec.predpc);
	}
	return 0;
}

max_size_t RAT::computePushValue() const {
	if (classifyOf(*this) == IssType::Link) {
		return to_unsigned(dec.pc) + 4;
	}
	return 0;
}

max_size_t RAT::computePushVR() const {
	return classifyOf(*this) == IssType::Link ? 1u : 0u;
}

max_size_t RAT::computePushCR() const {
	return classifyOf(*this) == IssType::Halt ? 1u : 0u;
}

max_size_t RAT::computePushCkpt() const {
	auto t = classifyOf(*this);
	if (t == IssType::Link || t == IssType::Branch) {
		return to_unsigned(dec.ckpt);
	}
	return 0;
}

bool RAT::popDecValid() const {
	if (squash.valid || dec.valid == 0) {
		return false;
	}
	if (computePushValid()) {
		return true;
	}
	return classifyOf(*this) == IssType::Invalid;
}

max_size_t RAT::rs1Tag() const {
	return to_unsigned(rat_table[to_unsigned(dec.rs1)]);
}

max_size_t RAT::rs2Tag() const {
	return to_unsigned(rat_table[to_unsigned(dec.rs2)]);
}

bool RAT::rs1HasTag() const {
	return rs1Tag() != 0;
}

bool RAT::rs2HasTag() const {
	return rs2Tag() != 0;
}

bool RAT::intPushValid() const {
	auto t = classifyOf(*this);
	return (t == IssType::Integer || t == IssType::Link) && computePushValid();
}

max_size_t RAT::intPushOp() const {
	auto t = classifyOf(*this);
	if (t != IssType::Integer && t != IssType::Link) {
		return static_cast<max_size_t>(Operation::OP_INVALID);
	}
	Instruct tmp;
	tmp.type = static_cast<RISC_V>(to_unsigned(dec.type));
	tmp.opcode = static_cast<int>(to_unsigned(dec.opcode));
	tmp.funct3 = static_cast<int>(to_unsigned(dec.funct3));
	tmp.funct7 = static_cast<int>(to_unsigned(dec.funct7));
	return static_cast<max_size_t>(decodeOp(tmp));
}

max_size_t RAT::intPushVj() const {
	if (intPushValid() == false) {
		return 0;
	}
	switch (static_cast<RISC_V>(to_unsigned(dec.type))) {
	case RISC_V::U: // AUIPC: vj = pc; LUI: vj unused (0)
		return to_unsigned(dec.opcode) == 0b0010111u ? to_unsigned(dec.pc) : 0u;
	case RISC_V::J: // JAL: vj = pc
		return to_unsigned(dec.pc);
	default: // R / I / Istar / JALR
		return resolveRs1(*this).value;
	}
}

max_size_t RAT::intPushQj() const {
	if (intPushValid() == false) {
		return 0;
	}
	auto t = static_cast<RISC_V>(to_unsigned(dec.type));
	if (t == RISC_V::U || t == RISC_V::J) {
		return 0; // pc-driven, always ready
	}
	return resolveRs1(*this).pending;
}

max_size_t RAT::intPushVk() const {
	if (intPushValid() == false) {
		return 0;
	}
	if (static_cast<RISC_V>(to_unsigned(dec.type)) == RISC_V::R) {
		return resolveRs2(*this).value;
	}
	return to_unsigned(dec.imm); // imm_as_vk (CPU.cpp:94-95)
}

max_size_t RAT::intPushQk() const {
	if (intPushValid() == false) {
		return 0;
	}
	if (static_cast<RISC_V>(to_unsigned(dec.type)) != RISC_V::R) {
		return 0;
	}
	return resolveRs2(*this).pending;
}

max_size_t RAT::intPushRobTag() const {
	if (intPushValid() == false) {
		return 0;
	}
	return to_unsigned(rob.rob_tag) + 1; // tag allocated this cycle (CPU.cpp:126)
}

bool RAT::loadPushValid() const {
	return classifyOf(*this) == IssType::Load && computePushValid();
}

max_size_t RAT::loadPushOp() const {
	return loadPushValid() ? static_cast<max_size_t>(Operation::Load) : 0;
}

max_size_t RAT::loadPushVj() const {
	return loadPushValid() ? resolveRs1(*this).value : 0;
}

max_size_t RAT::loadPushQj() const {
	return loadPushValid() ? resolveRs1(*this).pending : 0;
}

max_size_t RAT::loadPushVk() const {
	return loadPushValid() ? to_unsigned(dec.imm) : 0; // vk = imm (CPU.cpp:257)
}

max_size_t RAT::loadPushQk() const {
	return 0; // imm always ready (CPU.cpp:257)
}

max_size_t RAT::loadPushRobTag() const {
	if (loadPushValid() == false) {
		return 0;
	}
	return to_unsigned(rob.rob_tag) + 1;
}

bool RAT::branchPushValid() const {
	return classifyOf(*this) == IssType::Branch && computePushValid();
}

max_size_t RAT::branchPushOp() const {
	if (branchPushValid() == false) {
		return 0;
	}
	Instruct tmp;
	tmp.type = static_cast<RISC_V>(to_unsigned(dec.type));
	tmp.opcode = static_cast<int>(to_unsigned(dec.opcode));
	tmp.funct3 = static_cast<int>(to_unsigned(dec.funct3));
	tmp.funct7 = static_cast<int>(to_unsigned(dec.funct7));
	return static_cast<max_size_t>(decodeOp(tmp));
}

max_size_t RAT::branchPushVj() const {
	return branchPushValid() ? resolveRs1(*this).value : 0;
}

max_size_t RAT::branchPushQj() const {
	return branchPushValid() ? resolveRs1(*this).pending : 0;
}

max_size_t RAT::branchPushVk() const {
	return branchPushValid() ? resolveRs2(*this).value : 0;
}

max_size_t RAT::branchPushQk() const {
	return branchPushValid() ? resolveRs2(*this).pending : 0;
}

max_size_t RAT::branchPushImm() const {
	return branchPushValid() ? to_unsigned(dec.imm) : 0;
}

max_size_t RAT::branchPushPC() const {
	return branchPushValid() ? to_unsigned(dec.pc) : 0;
}

max_size_t RAT::branchPushRobTag() const {
	if (branchPushValid() == false) {
		return 0;
	}
	return to_unsigned(rob.rob_tag) + 1;
}

bool RAT::storeAddrValid() const {
	return classifyOf(*this) == IssType::Store && computePushValid();
}

max_size_t RAT::storeAddrOp() const {
	return storeAddrValid() ? static_cast<max_size_t>(Operation::Store) : 0;
}

max_size_t RAT::storeAddrVj() const {
	return storeAddrValid() ? resolveRs1(*this).value : 0;
}

max_size_t RAT::storeAddrQj() const {
	return storeAddrValid() ? resolveRs1(*this).pending : 0;
}

max_size_t RAT::storeAddrVk() const {
	return storeAddrValid() ? to_unsigned(dec.imm) : 0; // vk = imm (CPU.cpp:309)
}

max_size_t RAT::storeAddrRobTag() const {
	if (storeAddrValid() == false) {
		return 0;
	}
	return to_unsigned(rob.rob_tag) + 1;
}

bool RAT::storeOperandValid() const {
	return storeAddrValid(); // paired push of the same instruction
}

max_size_t RAT::storeOperandValue() const {
	return storeOperandValid() ? resolveRs2(*this).value : 0;
}

max_size_t RAT::storeOperandQ() const {
	return storeOperandValid() ? resolveRs2(*this).pending : 0;
}

max_size_t RAT::storeOperandRobTag() const {
	return storeAddrRobTag();
}

bool RAT::lsqPushValid() const {
	auto t = classifyOf(*this);
	return (t == IssType::Load || t == IssType::Store) && computePushValid();
}

max_size_t RAT::lsqPushIsLoad() const {
	return classifyOf(*this) == IssType::Load ? 1u : 0u;
}

max_size_t RAT::lsqPushNBytes() const { // funct3 switch, ports CPU.cpp:395-417/429-441
	if (lsqPushValid() == false) {
		return 0;
	}
	switch (to_unsigned(dec.funct3)) {
	case 0b000u:
		return 1;
	case 0b001u:
		return 2;
	case 0b010u:
		return 4;
	case 0b100u:
		return lsqPushIsLoad() ? 1u : 4u; // lbu: load only
	case 0b101u:
		return lsqPushIsLoad() ? 2u : 4u; // lhu: load only
	default:
		return 4;
	}
}

max_size_t RAT::lsqPushIsUnsigned() const {
	if (lsqPushValid() == false) {
		return 0;
	}
	auto f3 = to_unsigned(dec.funct3);
	return lsqPushIsLoad() && (f3 == 0b100u || f3 == 0b101u) ? 1u : 0u;
}

max_size_t RAT::lsqPushRobTag() const {
	if (lsqPushValid() == false) {
		return 0;
	}
	return to_unsigned(rob.rob_tag) + 1;
}

max_size_t RAT::ratRead(int reg) const {
	return reg >= 0 && reg < REGISTER_CAP ? to_unsigned(rat_table[reg]) : 0;
}

void RAT::work() {
	++g_cyc;
	if (squash.valid) {
		// RAT repair (ports CPU.cpp:1014-1034): roll renames younger than the
		// squash point back to the newest in-order writer (or 0 = no rename)
		if (rob_ptr) {
			auto sqTag = to_unsigned(squash.tag);
			for (int reg = 0; reg < REGISTER_CAP; ++reg) {
				if (to_unsigned(rat_table[reg]) <= sqTag) {
					continue;
				}
				bool repaired = false;
				max_size_t repairTag = 0;
				for (int idx = static_cast<int>(to_unsigned(rob_ptr->head));
					 idx != static_cast<int>(to_unsigned(rob_ptr->tail));
					 idx = (idx + 1) & 0x3F) {
					auto type = rob_ptr->getType(idx);
					if ((type == ROBType::REGISTER || type == ROBType::LINK)
						&& rob_ptr->getTag(idx) <= sqTag
						&& static_cast<int>(rob_ptr->getDest(idx)) == reg) {
						// no break: last (youngest in-order) match wins,
						// aligning the original walk
						repairTag = rob_ptr->getTag(idx);
						repaired = true;
					}
				}
				if (repaired) {
					rat_table[reg] <= repairTag;
				} else {
					rat_table[reg] <= 0;
				}
			}
		}
		return;
	}
	// commitPort: head committed REGISTER/LINK and RAT still points to it
	// (aligns CPU.cpp:358-367)
	bool commit_valid = false;
	max_size_t commit_reg = 0;
	if (rob.head_commit_ready) {
		auto htype = static_cast<ROBType>(to_unsigned(rob.head_type));
		if (htype == ROBType::REGISTER || htype == ROBType::LINK) {
			if (rat_table[to_unsigned(rob.head_dest)]
				== to_unsigned(rob.head_tag)) {
				commit_valid = true;
				commit_reg = to_unsigned(rob.head_dest);
			}
		}
	}
	// issuePort: allocate rename for the head instruction. Branch/store carry
	// garbage rd fields and never rename (CPU.cpp:227 issue_B / 354 issue_Store
	// return valid=false); renaming them would leave dead qj's on the garbage
	// dest register.
	auto iss = classifyOf(*this);
	bool issue_valid = computePushValid();
	max_size_t issue_reg = to_unsigned(dec.rd);
	bool issue_write = issue_valid && issue_reg != 0 // x0: no rename (CPU.cpp:133)
		&& iss != IssType::Branch && iss != IssType::Store;
	max_size_t issue_tag = issue_valid ? to_unsigned(rob.rob_tag) + 1 : 0;
	{
		if (to_unsigned(dec.pc) >= 4088u && to_unsigned(dec.pc) <= 5660u) {
			fprintf(stderr, "R cyc=%llu pc=%llu rd=%llu iv=%d iw=%d tag=%llu a0=%llu\n",
					g_cyc, to_unsigned(dec.pc), issue_reg,
					static_cast<int>(issue_valid), static_cast<int>(issue_write),
					issue_tag, to_unsigned(rat_table[10]));
		}
	}
	// RATSEL (ported from Arbiter.hpp:16-23; commit clears to 0)
	bool commit_write = commit_valid;
	if (issue_write && commit_valid && issue_reg == commit_reg) {
		commit_write = false; // issue overwrites commit on the same reg
	}
	if (commit_write) {
		rat_table[commit_reg] <= 0;
	}
	if (issue_write) {
		rat_table[issue_reg] <= issue_tag;
	}
}

} // namespace dark
