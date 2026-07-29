#include "../include/CPU.hpp"
#include <cstdio>

struct TestState {
  int passed;
  int failed;
};

static TestState check(TestState ts, bool cond, const char *msg) {
  if (cond) {
    ts.passed++;
  } else {
    ts.failed++;
    printf("  FAIL: %s\n", msg);
  }
  return ts;
}

static TestState check_eq(TestState ts, int32_t expected, int32_t actual,
                          const char *msg) {
  if (expected == actual) {
    ts.passed++;
  } else {
    ts.failed++;
    printf("  FAIL: %s (expected %d, got %d)\n", msg, expected, actual);
  }
  return ts;
}

static void init_all_RS(systemState &state) {
  for (int i = 0; i < INTEGERRS_CAP; i++) {
    state.IntegerRS[i].free = true;
    state.IntegerRS[i].qj = -1;
    state.IntegerRS[i].qk = -1;
    state.IntegerRS[i].ROB_dest = ~0u >> 1;
    state.IntegerRS[i].vj = 0;
    state.IntegerRS[i].vk = 0;
  }
}

void run_ALU_tomasulo_tests() {
  TestState ts = {0, 0};

  printf("=== ALU Operation Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    ts = check_eq(ts, 30, cpu.ALU(10, 20, ADD), "ADD 10+20");
    ts = check_eq(ts, -10, cpu.ALU(10, 20, SUB), "SUB 10-20");
    ts = check_eq(ts, 30, cpu.ALU(10, 20, XOR), "XOR 10^20");
    ts = check_eq(ts, 30, cpu.ALU(10, 20, OR), "OR 10|20");
    ts = check_eq(ts, 0, cpu.ALU(10, 20, AND), "AND 10&20");
    ts = check_eq(ts, 40, cpu.ALU(10, 2, SL), "SL 10<<2");
    ts = check_eq(ts, 2, cpu.ALU(10, 2, SRL), "SRL 10>>2");
    ts = check_eq(ts, 2, cpu.ALU(10, 2, SRA), "SRA 10>>2");
    ts = check_eq(ts, 1, cpu.ALU(10, 20, SLT), "SLT 10<20");
    ts = check_eq(ts, 0, cpu.ALU(20, 10, SLT), "SLT 20<10");
    ts = check_eq(ts, 1, cpu.ALU(-10, 20, SLT), "SLT -10<20");
    ts = check_eq(ts, 1, cpu.ALU(10, 20, SLTU), "SLTU 10<20");
    ts = check_eq(ts, 0, cpu.ALU(-1, 1, SLTU), "SLTU unsigned cmp");
    ts = check_eq(ts, 0, cpu.ALU(10, 20, OP_INVALID), "OP_INVALID returns 0");

    ts = check_eq(ts, 0, cpu.ALU(0, 0, ADD), "ADD 0+0");
    ts = check_eq(ts, -1, cpu.ALU(0, 1, SUB), "SUB 0-1");
    ts = check_eq(ts, 0, cpu.ALU(0, 0, XOR), "XOR 0^0");
    ts = check_eq(ts, -1, cpu.ALU(-1, 0, OR), "OR -1|0");
    ts = check_eq(ts, 0, cpu.ALU(0, -1, AND), "AND 0&-1");

    ts = check_eq(ts, 1, cpu.ALU(1, 32, SL), "SL shift 32 masked to 0");
    ts = check_eq(ts, 2, cpu.ALU(1, 33, SL), "SL shift 33 masked to 1");
    ts = check_eq(ts, 1, cpu.ALU(1, 32, SRL), "SRL shift 32 masked to 0");
    ts = check_eq(ts, -1, cpu.ALU(-1, 31, SRA), "SRA -1>>31 sign extend");
    ts = check_eq(ts, 0, cpu.ALU(5, 5, SLT), "SLT equal");
    ts = check_eq(ts, 0, cpu.ALU(5, 5, SLTU), "SLTU equal");

    int32_t max_int = 2147483647;
    int32_t min_int = static_cast<int32_t>(0x80000000);
    ts = check_eq(ts, max_int, cpu.ALU(max_int, 0, ADD), "ADD max_int+0");
    ts = check_eq(ts, min_int, cpu.ALU(max_int, 1, ADD), "ADD overflow");
    ts = check_eq(ts, min_int, cpu.ALU(min_int, 0, SUB), "SUB min_int-0");
    ts = check_eq(ts, max_int, cpu.ALU(min_int, 1, SUB), "SUB min_int-1");
    ts = check_eq(ts, -2, cpu.ALU(-1, -1, ADD), "ADD -1+-1");
  }

  printf("\n=== decodeOp Tests ===\n");
  {
    Instruct inst;
    inst.type = R;
    inst.funct3 = 0;
    inst.funct7 = 0;
    ts = check_eq(ts, ADD, CPU::decodeOp(inst), "decodeOp R ADD");
    inst.funct7 = 0b0100000;
    ts = check_eq(ts, SUB, CPU::decodeOp(inst), "decodeOp R SUB");
    inst.funct3 = 0b001;
    inst.funct7 = 0;
    ts = check_eq(ts, SL, CPU::decodeOp(inst), "decodeOp R SL");
    inst.funct3 = 0b010;
    ts = check_eq(ts, SLT, CPU::decodeOp(inst), "decodeOp R SLT");
    inst.funct3 = 0b011;
    ts = check_eq(ts, SLTU, CPU::decodeOp(inst), "decodeOp R SLTU");
    inst.funct3 = 0b100;
    ts = check_eq(ts, XOR, CPU::decodeOp(inst), "decodeOp R XOR");
    inst.funct3 = 0b101;
    ts = check_eq(ts, SRL, CPU::decodeOp(inst), "decodeOp R SRL");
    inst.funct7 = 0b0100000;
    ts = check_eq(ts, SRA, CPU::decodeOp(inst), "decodeOp R SRA");
    inst.funct3 = 0b110;
    inst.funct7 = 0;
    ts = check_eq(ts, OR, CPU::decodeOp(inst), "decodeOp R OR");
    inst.funct3 = 0b111;
    ts = check_eq(ts, AND, CPU::decodeOp(inst), "decodeOp R AND");
  }
  {
    Instruct inst;
    inst.type = I;
    inst.funct3 = 0b000;
    ts = check_eq(ts, ADD, CPU::decodeOp(inst), "decodeOp I ADDI");
    inst.funct3 = 0b010;
    ts = check_eq(ts, SLT, CPU::decodeOp(inst), "decodeOp I SLTI");
    inst.funct3 = 0b011;
    ts = check_eq(ts, SLTU, CPU::decodeOp(inst), "decodeOp I SLTIU");
    inst.funct3 = 0b100;
    ts = check_eq(ts, XOR, CPU::decodeOp(inst), "decodeOp I XORI");
    inst.funct3 = 0b110;
    ts = check_eq(ts, OR, CPU::decodeOp(inst), "decodeOp I ORI");
    inst.funct3 = 0b111;
    ts = check_eq(ts, AND, CPU::decodeOp(inst), "decodeOp I ANDI");
  }
  {
    Instruct inst;
    inst.type = Istar;
    inst.funct3 = 1;
    ts = check_eq(ts, SL, CPU::decodeOp(inst), "decodeOp Istar SLLI");
    inst.funct3 = 5;
    inst.funct7 = 0;
    ts = check_eq(ts, SRL, CPU::decodeOp(inst), "decodeOp Istar SRLI");
    inst.funct7 = 0b0100000;
    ts = check_eq(ts, SRA, CPU::decodeOp(inst), "decodeOp Istar SRAI");
  }
  {
    Instruct inst;
    ts = check_eq(ts, OP_INVALID, CPU::decodeOp(inst), "decodeOp invalid");
  }

  printf("\n=== Pipeline Execute Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);
    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = ADD;
    cpu.curCPUstate.IntegerRS[0].vj = 15;
    cpu.curCPUstate.IntegerRS[0].vk = 25;
    cpu.curCPUstate.IntegerRS[0].qj = -1;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = 1;
    cpu.curCPUstate.commonDataBus.is_valid = false;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, cpu.nextCPUstate.commonDataBus.is_valid,
               "execute CDB valid");
    ts = check_eq(ts, 40, cpu.nextCPUstate.commonDataBus.value,
                  "execute CDB value 15+25");
    ts = check_eq(ts, 1, cpu.nextCPUstate.commonDataBus.rob_mark,
                  "execute CDB rob_mark");
    ts = check(ts, cpu.nextCPUstate.IntegerRS[0].free, "execute RS freed");

    cpu.curCPUstate = cpu.nextCPUstate;
    init_all_RS(cpu.curCPUstate);
    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = SUB;
    cpu.curCPUstate.IntegerRS[0].vj = 0;
    cpu.curCPUstate.IntegerRS[0].vk = 0;
    cpu.curCPUstate.IntegerRS[0].qj = 2;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = 2;
    cpu.curCPUstate.commonDataBus.is_valid = false;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, !cpu.nextCPUstate.commonDataBus.is_valid,
               "execute skip non-ready RS");

    cpu.curCPUstate = cpu.nextCPUstate;
    init_all_RS(cpu.curCPUstate);
    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = OR;
    cpu.curCPUstate.IntegerRS[0].vj = 0xFF;
    cpu.curCPUstate.IntegerRS[0].vk = 0x0F;
    cpu.curCPUstate.IntegerRS[0].qj = -1;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = 4;
    cpu.curCPUstate.IntegerRS[1].free = false;
    cpu.curCPUstate.IntegerRS[1].op = AND;
    cpu.curCPUstate.IntegerRS[1].vj = 0xFF;
    cpu.curCPUstate.IntegerRS[1].vk = 0x0F;
    cpu.curCPUstate.IntegerRS[1].qj = -1;
    cpu.curCPUstate.IntegerRS[1].qk = -1;
    cpu.curCPUstate.IntegerRS[1].ROB_dest = 3;
    cpu.curCPUstate.commonDataBus.is_valid = false;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check_eq(ts, 3, cpu.nextCPUstate.commonDataBus.rob_mark,
                  "execute picks oldest ready RS");
    ts = check_eq(ts, 0x0F, cpu.nextCPUstate.commonDataBus.value,
                  "execute oldest RS AND result");
  }

  printf("\n=== Pipeline writeBack Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);
    int tag1 = cpu.curCPUstate.CPUROB.push(ROBEntry(REGISTER));
    int tag2 = cpu.curCPUstate.CPUROB.push(ROBEntry(REGISTER));
    int tag3 = cpu.curCPUstate.CPUROB.push(ROBEntry(REGISTER));
    int tag_test = cpu.curCPUstate.CPUROB.push(ROBEntry(REGISTER));
    int tag4 = cpu.curCPUstate.CPUROB.push(ROBEntry(REGISTER));

    cpu.curCPUstate.commonDataBus.is_valid = true;
    cpu.curCPUstate.commonDataBus.value = 42;
    cpu.curCPUstate.commonDataBus.rob_mark = tag_test;

    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].qj = tag_test;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].vj = 0;
    cpu.curCPUstate.IntegerRS[0].vk = 0;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = tag4;

    cpu.curCPUstate.IntegerRS[1].free = false;
    cpu.curCPUstate.IntegerRS[1].qj = -1;
    cpu.curCPUstate.IntegerRS[1].qk = tag_test;
    cpu.curCPUstate.IntegerRS[1].vj = 0;
    cpu.curCPUstate.IntegerRS[1].vk = 0;
    cpu.curCPUstate.IntegerRS[1].ROB_dest = tag4;

    cpu.curCPUstate.IntegerRS[2].free = false;
    cpu.curCPUstate.IntegerRS[2].qj = tag1;
    cpu.curCPUstate.IntegerRS[2].qk = tag2;
    cpu.curCPUstate.IntegerRS[2].vj = 0;
    cpu.curCPUstate.IntegerRS[2].vk = 0;
    cpu.curCPUstate.IntegerRS[2].ROB_dest = tag4;

    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.writeBack();

    ts = check_eq(ts, 42, cpu.nextCPUstate.IntegerRS[0].vj,
                  "writeBack updated vj from qj");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[0].qj,
                  "writeBack cleared qj");
    ts = check_eq(ts, 42, cpu.nextCPUstate.IntegerRS[1].vk,
                  "writeBack updated vk from qk");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[1].qk,
                  "writeBack cleared qk");
    ts = check_eq(ts, 0, cpu.nextCPUstate.IntegerRS[2].vj,
                  "writeBack no match qj unchanged");
    ts = check_eq(ts, tag1, cpu.nextCPUstate.IntegerRS[2].qj,
                  "writeBack no match qj kept");
    ts = check_eq(ts, 0, cpu.nextCPUstate.IntegerRS[2].vk,
                  "writeBack no match qk unchanged");
    ts = check_eq(ts, tag2, cpu.nextCPUstate.IntegerRS[2].qk,
                  "writeBack no match qk kept");

    cpu.curCPUstate = cpu.nextCPUstate;
    init_all_RS(cpu.curCPUstate);
    cpu.curCPUstate.commonDataBus.is_valid = false;
    cpu.curCPUstate.IntegerRS[0].qj = tag_test;
    cpu.curCPUstate.IntegerRS[0].vj = 0;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.writeBack();

    ts = check_eq(ts, 0, cpu.nextCPUstate.IntegerRS[0].vj,
                  "writeBack skip invalid CDB vj");
    ts = check_eq(ts, tag_test, cpu.nextCPUstate.IntegerRS[0].qj,
                  "writeBack skip invalid CDB qj kept");
  }

  printf("\n=== Pipeline Commit Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.nextCPUstate);
    ROBEntry entry(REGISTER);
    entry.dest = 10;
    entry.value = 99;
    int tag = cpu.nextCPUstate.CPUROB.push(entry);
    int idx = cpu.nextCPUstate.CPUROB.getIndex(tag);
    cpu.nextCPUstate.CPUROB.writeROB(99, idx, Ready);
    cpu.curCPUstate = cpu.nextCPUstate;

    cpu.commit();

    ts = check_eq(ts, 99, cpu.nextCPUstate.reg[10].read(),
                  "commit wrote register 10");
    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[0].read(),
                  "commit x0 still 0");
    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[5].read(),
                  "commit other reg unchanged");

    cpu.curCPUstate = cpu.nextCPUstate;
    ROBEntry entry2(REGISTER);
    entry2.dest = 11;
    entry2.value = 77;
    int tag2 = cpu.nextCPUstate.CPUROB.push(entry2);
    int idx2 = cpu.nextCPUstate.CPUROB.getIndex(tag2);
    cpu.nextCPUstate.CPUROB.writeROB(77, idx2, Waiting);
    cpu.curCPUstate = cpu.nextCPUstate;

    cpu.commit();

    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[11].read(),
                  "commit skip non-ready ROB");
  }

  printf("\n=== Full Pipeline Cycle Test ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);
    ROBEntry entry(REGISTER);
    entry.dest = 5;
    int tag = cpu.curCPUstate.CPUROB.push(entry);
    int idx = cpu.curCPUstate.CPUROB.getIndex(tag);
    cpu.curCPUstate.CPUROB.writeROB(0, idx, Executing);

    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = ADD;
    cpu.curCPUstate.IntegerRS[0].vj = 10;
    cpu.curCPUstate.IntegerRS[0].vk = 5;
    cpu.curCPUstate.IntegerRS[0].qj = -1;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = tag;
    cpu.curCPUstate.commonDataBus.is_valid = false;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, cpu.nextCPUstate.commonDataBus.is_valid,
               "full cycle execute CDB valid");
    ts = check_eq(ts, 15, cpu.nextCPUstate.commonDataBus.value,
                  "full cycle ALU result");
    ts = check_eq(ts, tag, cpu.nextCPUstate.commonDataBus.rob_mark,
                  "full cycle CDB rob_mark");

    cpu.curCPUstate = cpu.nextCPUstate;
    init_all_RS(cpu.curCPUstate);
    cpu.curCPUstate.IntegerRS[1].free = false;
    cpu.curCPUstate.IntegerRS[1].qj = tag;
    cpu.curCPUstate.IntegerRS[1].qk = -1;
    cpu.curCPUstate.IntegerRS[1].vj = 0;
    cpu.curCPUstate.IntegerRS[1].vk = 0;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.writeBack();

    ts = check_eq(ts, 15, cpu.nextCPUstate.IntegerRS[1].vj,
                  "full cycle writeBack updated waiting RS");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[1].qj,
                  "full cycle writeBack cleared qj");

    cpu.curCPUstate = cpu.nextCPUstate;
    int write_idx = cpu.curCPUstate.CPUROB.getIndex(tag);
    cpu.nextCPUstate.CPUROB.writeROB(15, write_idx, Ready);

    cpu.commit();

    ts = check_eq(ts, 15, cpu.nextCPUstate.reg[5].read(),
                  "full cycle commit wrote reg 5");
  }

  printf("\n========================================\n");
  printf("ALU Tomasulo Unit Tests: %d passed, %d failed\n", ts.passed,
         ts.failed);
  if (ts.failed > 0) {
    printf("SOME TESTS FAILED!\n");
  } else {
    printf("ALL TESTS PASSED!\n");
  }
  printf("========================================\n");
}

int main() {
  run_ALU_tomasulo_tests();
  return 0;
}