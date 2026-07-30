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

static void write_inst(Memory &mem, uint32_t addr, uint32_t inst) {
  mem.write_data(addr, inst & 0xFF);
  mem.write_data(addr + 1, (inst >> 8) & 0xFF);
  mem.write_data(addr + 2, (inst >> 16) & 0xFF);
  mem.write_data(addr + 3, (inst >> 24) & 0xFF);
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

  // ========== ALU Direct Tests ==========
  printf("\n=== ALU Direct Tests ===\n");
  {
    ALU alu;

    alu.ALUExecute(10, 20, ADD, 1);
    auto r = alu.pop();
    ts = check_eq(ts, 30, r.value, "ADD 10+20");
    ts = check_eq(ts, 1, r.robTag, "ADD robTag");

    alu.ALUExecute(10, 20, SUB, 2);
    r = alu.pop();
    ts = check_eq(ts, -10, r.value, "SUB 10-20");

    alu.ALUExecute(10, 20, XOR, 3);
    r = alu.pop();
    ts = check_eq(ts, 30, r.value, "XOR 10^20");

    alu.ALUExecute(10, 20, OR, 4);
    r = alu.pop();
    ts = check_eq(ts, 30, r.value, "OR 10|20");

    alu.ALUExecute(10, 20, AND, 5);
    r = alu.pop();
    ts = check_eq(ts, 0, r.value, "AND 10&20");

    alu.ALUExecute(10, 2, SL, 6);
    r = alu.pop();
    ts = check_eq(ts, 40, r.value, "SL 10<<2");

    alu.ALUExecute(10, 2, SRL, 7);
    r = alu.pop();
    ts = check_eq(ts, 2, r.value, "SRL 10>>2");

    alu.ALUExecute(10, 2, SRA, 8);
    r = alu.pop();
    ts = check_eq(ts, 2, r.value, "SRA 10>>2");

    alu.ALUExecute(10, 20, SLT, 9);
    r = alu.pop();
    ts = check_eq(ts, 1, r.value, "SLT 10<20");

    alu.ALUExecute(20, 10, SLT, 10);
    r = alu.pop();
    ts = check_eq(ts, 0, r.value, "SLT 20<10");

    alu.ALUExecute(-10, 20, SLT, 11);
    r = alu.pop();
    ts = check_eq(ts, 1, r.value, "SLT -10<20");

    alu.ALUExecute(10, 20, SLTU, 12);
    r = alu.pop();
    ts = check_eq(ts, 1, r.value, "SLTU 10<20");

    alu.ALUExecute(-1, 1, SLTU, 13);
    r = alu.pop();
    ts = check_eq(ts, 0, r.value, "SLTU unsigned cmp");

    alu.ALUExecute(10, 20, OP_INVALID, 14);
    r = alu.pop();
    ts = check_eq(ts, 0, r.value, "OP_INVALID returns 0");

    alu.ALUExecute(0, 0, ADD, 15);
    r = alu.pop();
    ts = check_eq(ts, 0, r.value, "ADD 0+0");

    alu.ALUExecute(0, 1, SUB, 16);
    r = alu.pop();
    ts = check_eq(ts, -1, r.value, "SUB 0-1");

    alu.ALUExecute(1, 32, SL, 17);
    r = alu.pop();
    ts = check_eq(ts, 1, r.value, "SL shift 32 masked to 0");

    alu.ALUExecute(1, 33, SL, 18);
    r = alu.pop();
    ts = check_eq(ts, 2, r.value, "SL shift 33 masked to 1");

    alu.ALUExecute(1, 32, SRL, 19);
    r = alu.pop();
    ts = check_eq(ts, 1, r.value, "SRL shift 32 masked to 0");

    alu.ALUExecute(-1, 31, SRA, 20);
    r = alu.pop();
    ts = check_eq(ts, -1, r.value, "SRA -1>>31 sign extend");

    alu.ALUExecute(-16, 2, SRA, 26);
    r = alu.pop();
    ts = check_eq(ts, -4, r.value, "SRA -16>>2=-4");

    alu.ALUExecute(-16, 1, SRA, 27);
    r = alu.pop();
    ts = check_eq(ts, -8, r.value, "SRA -16>>1=-8");

    alu.ALUExecute(-8, 3, SRA, 28);
    r = alu.pop();
    ts = check_eq(ts, -1, r.value, "SRA -8>>3=-1");

    alu.ALUExecute(-1, -1, ADD, 21);
    r = alu.pop();
    ts = check_eq(ts, -2, r.value, "ADD -1+-1");

    int32_t max_int = 2147483647;
    int32_t min_int = static_cast<int32_t>(0x80000000);
    alu.ALUExecute(max_int, 0, ADD, 22);
    r = alu.pop();
    ts = check_eq(ts, max_int, r.value, "ADD max_int+0");

    alu.ALUExecute(max_int, 1, ADD, 23);
    r = alu.pop();
    ts = check_eq(ts, min_int, r.value, "ADD overflow");

    alu.ALUExecute(min_int, 0, SUB, 24);
    r = alu.pop();
    ts = check_eq(ts, min_int, r.value, "SUB min_int-0");

    alu.ALUExecute(min_int, 1, SUB, 25);
    r = alu.pop();
    ts = check_eq(ts, max_int, r.value, "SUB min_int-1");
  }

  // ========== decodeOp Tests ==========
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

  // ========== R-type Pipeline Tests ==========
  printf("\n=== R-type Pipeline Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x002082B3); // add x5, x1, x2

    cpu.curCPUstate.reg[1].write(10);
    cpu.curCPUstate.reg[2].write(20);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "R-type issued to RS");
    if (idx != -1) {
      auto &rs = cpu.nextCPUstate.IntegerRS[idx];
      ts = check_eq(ts, ADD, rs.op, "R-type RS op=ADD");
      ts = check_eq(ts, 10, rs.vj, "R-type RS vj=10");
      ts = check_eq(ts, 20, rs.vk, "R-type RS vk=20");
      ts = check_eq(ts, -1, rs.qj, "R-type RS qj=-1");
      ts = check_eq(ts, -1, rs.qk, "R-type RS qk=-1");
      ts = check(ts, !rs.free, "R-type RS not free");
    }

    ts = check(ts, !cpu.nextCPUstate.ROBModule.isFull(),
               "R-type ROB not full");
    ts = check_eq(ts, -1, cpu.curCPUstate.RegisterTable[5],
                  "R-type old RegisterTable[5]=-1");
    ts = check(ts, cpu.nextCPUstate.RegisterTable[5] != -1,
               "R-type RegisterTable[5] updated");

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, !cpu.nextCPUstate.ALUModule.isEmpty(),
               "R-type ALU has result");
    ts = check(ts, cpu.nextCPUstate.IntegerRS[0].free,
               "R-type RS freed after execute");

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 30, result.value, "R-type ALU result 10+20=30");

    cpu.curCPUstate = cpu.nextCPUstate;
    int tag = cpu.curCPUstate.RegisterTable[5];
    cpu.curCPUstate.ALUModule.ALUExecute(10, 20, ADD, tag);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.writeBack();

    ts = check(ts, cpu.curCPUstate.ALUModule.isEmpty(),
               "R-type ALU buffer consumed");
    ts = check_eq(ts, Ready, cpu.nextCPUstate.ROBModule.peek().state,
                  "R-type ROB state=Ready");

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.commit();

    ts = check_eq(ts, 30, cpu.nextCPUstate.reg[5].read(),
                  "R-type commit reg[5]=30");
    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[0].read(),
                  "R-type x0 still 0");
  }

  // ========== I-type Pipeline Tests ==========
  printf("\n=== I-type Pipeline Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x06408793); // addi x15, x1, 100

    cpu.curCPUstate.reg[1].write(50);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "I-type issued to RS");
    if (idx != -1) {
      auto &rs = cpu.nextCPUstate.IntegerRS[idx];
      ts = check_eq(ts, ADD, rs.op, "I-type RS op=ADD");
      ts = check_eq(ts, 50, rs.vj, "I-type RS vj=50");
      ts = check_eq(ts, 100, rs.vk, "I-type RS vk=imm=100");
      ts = check_eq(ts, -1, rs.qj, "I-type RS qj=-1");
      ts = check_eq(ts, -1, rs.qk, "I-type RS qk=-1");
    }

    ts = check(ts, cpu.nextCPUstate.RegisterTable[15] != -1,
               "I-type RegisterTable[15] updated");

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 150, result.value, "I-type ADDI 50+100=150");

    cpu.curCPUstate = cpu.nextCPUstate;
    int tag = cpu.curCPUstate.RegisterTable[15];
    cpu.curCPUstate.ALUModule.ALUExecute(50, 100, ADD, tag);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.writeBack();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.commit();

    ts = check_eq(ts, 150, cpu.nextCPUstate.reg[15].read(),
                  "I-type commit reg[15]=150");
  }

  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x0FF0C813); // xori x16, x1, 0xFF

    cpu.curCPUstate.reg[1].write(0x0F0);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "XORI issued");
    if (idx != -1) {
      auto &rs = cpu.nextCPUstate.IntegerRS[idx];
      ts = check_eq(ts, XOR, rs.op, "XORI RS op=XOR");
      ts = check_eq(ts, 0x0F0, rs.vj, "XORI RS vj=0xF0");
      ts = check_eq(ts, 0xFF, rs.vk, "XORI RS vk=0xFF");
    }

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 0x0F, result.value, "XORI 0xF0^0xFF=0x0F");
  }

  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x0FF0C813); // xori x16, x1, 0xFF
    write_inst(cpu.curCPUstate.InstructMem, 4, 0x00F0E893); // ori x17, x1, 0x0F
    write_inst(cpu.curCPUstate.InstructMem, 8, 0x00F0F913); // andi x18, x1, 0x0F
    write_inst(cpu.curCPUstate.InstructMem, 12, 0x06412993); // slti x19, x1, 100
    write_inst(cpu.curCPUstate.InstructMem, 16, 0x06413A13); // sltiu x20, x1, 100

    cpu.curCPUstate.reg[1].write(10);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.execute();
    auto r1 = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 10 ^ 0xFF, r1.value, "XORI 10^0xFF");

    cpu.programCounter = 4;
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.issue();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.execute();
    auto r2 = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 10 | 0x0F, r2.value, "ORI 10|0x0F");

    cpu.programCounter = 8;
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.issue();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.execute();
    auto r3 = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 10 & 0x0F, r3.value, "ANDI 10&0x0F");

    cpu.programCounter = 12;
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.issue();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.execute();
    auto r4 = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 1, r4.value, "SLTI 10<100");

    cpu.programCounter = 16;
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.issue();
    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.execute();
    auto r5 = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 1, r5.value, "SLTIU 10<100");
  }

  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0xFFF08293); // addi x5, x1, -1

    cpu.curCPUstate.reg[1].write(10);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "ADDI -1 issued");
    if (idx != -1) {
      ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[idx].vk,
                    "ADDI imm=-1 sign-extended");
    }

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 9, result.value, "ADDI 10+(-1)=9");
  }

  // ========== I*-type Pipeline Tests ==========
  printf("\n=== I*-type Pipeline Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x00309A93); // slli x21, x1, 3

    cpu.curCPUstate.reg[1].write(5);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "SLLI issued");
    if (idx != -1) {
      auto &rs = cpu.nextCPUstate.IntegerRS[idx];
      ts = check_eq(ts, SL, rs.op, "SLLI RS op=SL");
      ts = check_eq(ts, 5, rs.vj, "SLLI RS vj=5");
      ts = check_eq(ts, 3, rs.vk, "SLLI RS vk=shamt=3");
    }

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 40, result.value, "SLLI 5<<3=40");
  }

  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x0030DB13); // srli x22, x1, 3

    cpu.curCPUstate.reg[1].write(40);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "SRLI issued");
    if (idx != -1) {
      ts = check_eq(ts, SRL, cpu.nextCPUstate.IntegerRS[idx].op,
                    "SRLI RS op=SRL");
    }

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 5, result.value, "SRLI 40>>3=5");
  }

  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x4030DB93); // srai x23, x1, 3

    cpu.curCPUstate.reg[1].write(-16);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int idx = -1;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        idx = i;
        break;
      }
    }
    ts = check(ts, idx != -1, "SRAI issued");
    if (idx != -1) {
      ts = check_eq(ts, SRA, cpu.nextCPUstate.IntegerRS[idx].op,
                    "SRAI RS op=SRA");
    }

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, -2, result.value, "SRAI -16>>3=-2");
  }

  // ========== Tomasulo RAW Hazard Tests ==========
  printf("\n=== Tomasulo RAW Hazard Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x002082B3); // add x5, x1, x2
    write_inst(cpu.curCPUstate.InstructMem, 4, 0x005201B3); // add x3, x4, x5

    cpu.curCPUstate.reg[1].write(10);
    cpu.curCPUstate.reg[2].write(20);
    cpu.curCPUstate.reg[4].write(30);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int tag1 = cpu.nextCPUstate.RegisterTable[5];
    ts = check(ts, tag1 != -1, "RAW: issue inst1, dest tag set");

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.programCounter = 4;

    cpu.issue();

    bool found_raw = false;
    int raw_qk = -1;
    int raw_vj = 0;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free &&
          cpu.nextCPUstate.IntegerRS[i].qk != -1) {
        found_raw = true;
        raw_qk = cpu.nextCPUstate.IntegerRS[i].qk;
        raw_vj = cpu.nextCPUstate.IntegerRS[i].vj;
        break;
      }
    }
    ts = check(ts, found_raw, "RAW: inst2 has qk != -1");
    if (found_raw) {
      ts = check_eq(ts, tag1, raw_qk, "RAW: inst2 qk == inst1 ROB tag");
      ts = check_eq(ts, 30, raw_vj, "RAW: inst2 vj=30 (rs1 ready)");
    }
  }

  // ========== Tomasulo WAW Hazard Tests ==========
  printf("\n=== Tomasulo WAW Hazard Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    write_inst(cpu.curCPUstate.InstructMem, 0, 0x002082B3); // add x5, x1, x2
    write_inst(cpu.curCPUstate.InstructMem, 4, 0x004182B3); // add x5, x3, x4

    cpu.curCPUstate.reg[1].write(10);
    cpu.curCPUstate.reg[2].write(20);
    cpu.curCPUstate.reg[3].write(30);
    cpu.curCPUstate.reg[4].write(40);
    cpu.programCounter = 0;

    init_all_RS(cpu.curCPUstate);
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.issue();

    int tag1 = cpu.nextCPUstate.RegisterTable[5];
    ts = check(ts, tag1 != -1, "WAW: inst1 dest tag set");

    cpu.curCPUstate = cpu.nextCPUstate;
    cpu.nextCPUstate = cpu.curCPUstate;
    cpu.programCounter = 4;

    cpu.issue();

    int tag2 = cpu.nextCPUstate.RegisterTable[5];
    ts = check(ts, tag2 != -1 && tag2 != tag1,
               "WAW: inst2 overwrites RegisterTable");
    ts = check(ts, tag2 > tag1, "WAW: inst2 tag is newer than inst1 tag");

    int count = 0;
    for (int i = 0; i < INTEGERRS_CAP; i++) {
      if (!cpu.nextCPUstate.IntegerRS[i].free) {
        count++;
      }
    }
    ts = check_eq(ts, 2, count, "WAW: both instructions in RS");
  }

  // ========== Execute Oldest-First Tests ==========
  printf("\n=== Execute Oldest-First Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

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

    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, cpu.nextCPUstate.IntegerRS[1].free,
               "oldest: RS with ROB_dest=3 was freed");
    ts = check(ts, !cpu.nextCPUstate.IntegerRS[0].free,
               "oldest: RS with ROB_dest=4 still occupied");

    auto result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 0x0F, result.value, "oldest: AND 0xFF&0x0F=0x0F");
  }

  // ========== Circular ROB Age Tests ==========
  printf("\n=== Circular ROB Age Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);

    for (int i = 0; i < 63; i++) {
      int tag = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));
      int idx = cpu.curCPUstate.ROBModule.getIndex(tag);
      cpu.curCPUstate.ROBModule.writeROB(0, idx, Ready);
    }
    int tag_older = 63;
    for (int i = 0; i < 32; i++) {
      cpu.curCPUstate.ROBModule.pop();
    }

    int tag_newer = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));

    int idx_older = cpu.curCPUstate.ROBModule.getIndex(tag_older);
    int idx_newer = cpu.curCPUstate.ROBModule.getIndex(tag_newer);
    ts = check(ts, idx_older > idx_newer,
               "circular: older tag has larger index after wrap");
    ts = check(ts, tag_older < tag_newer,
               "circular: tags still monotonically increase");

    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = ADD;
    cpu.curCPUstate.IntegerRS[0].vj = 100;
    cpu.curCPUstate.IntegerRS[0].vk = 200;
    cpu.curCPUstate.IntegerRS[0].qj = -1;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = tag_newer;

    cpu.curCPUstate.IntegerRS[1].free = false;
    cpu.curCPUstate.IntegerRS[1].op = SUB;
    cpu.curCPUstate.IntegerRS[1].vj = 500;
    cpu.curCPUstate.IntegerRS[1].vk = 100;
    cpu.curCPUstate.IntegerRS[1].qj = -1;
    cpu.curCPUstate.IntegerRS[1].qk = -1;
    cpu.curCPUstate.IntegerRS[1].ROB_dest = tag_older;

    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, cpu.nextCPUstate.IntegerRS[1].free,
               "circular: older RS (smaller tag) freed after wrap");
    ts = check(ts, !cpu.nextCPUstate.IntegerRS[0].free,
               "circular: newer RS (larger tag) still occupied after wrap");

    auto alu_result = cpu.nextCPUstate.ALUModule.pop();
    ts = check_eq(ts, 400, alu_result.value, "circular: SUB 500-100=400");
    ts = check_eq(ts, tag_older, alu_result.robTag,
                  "circular: result tag matches older tag");
  }

  // ========== WriteBack Forwarding Tests ==========
  printf("\n=== WriteBack Forwarding Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);

    int tag1 = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));
    int tag2 = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));
    int tag_test = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));
    int tag4 = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));

    cpu.curCPUstate.ALUModule.ALUExecute(42, 0, ADD, tag_test);

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
                  "writeBack: updated vj from qj");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[0].qj,
                  "writeBack: cleared qj");
    ts = check_eq(ts, 42, cpu.nextCPUstate.IntegerRS[1].vk,
                  "writeBack: updated vk from qk");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[1].qk,
                  "writeBack: cleared qk");
    ts = check_eq(ts, 0, cpu.nextCPUstate.IntegerRS[2].vj,
                  "writeBack: no match vj unchanged");
    ts = check_eq(ts, tag1, cpu.nextCPUstate.IntegerRS[2].qj,
                  "writeBack: no match qj kept");
    ts = check_eq(ts, 0, cpu.nextCPUstate.IntegerRS[2].vk,
                  "writeBack: no match vk unchanged");
    ts = check_eq(ts, tag2, cpu.nextCPUstate.IntegerRS[2].qk,
                  "writeBack: no match qk kept");
  }

  // ========== Commit Tests ==========
  printf("\n=== Commit Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.nextCPUstate);
    ROBEntry entry(REGISTER);
    entry.dest = 10;
    entry.value = 99;
    int tag = cpu.nextCPUstate.ROBModule.push(entry);
    int idx = cpu.nextCPUstate.ROBModule.getIndex(tag);
    cpu.nextCPUstate.ROBModule.writeROB(99, idx, Ready);
    cpu.curCPUstate = cpu.nextCPUstate;

    cpu.commit();

    ts = check_eq(ts, 99, cpu.nextCPUstate.reg[10].read(),
                  "commit: wrote register 10");
    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[0].read(),
                  "commit: x0 still 0");
  }

  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.nextCPUstate);
    ROBEntry entry(REGISTER);
    entry.dest = 11;
    entry.value = 77;
    int tag = cpu.nextCPUstate.ROBModule.push(entry);
    int idx = cpu.nextCPUstate.ROBModule.getIndex(tag);
    cpu.nextCPUstate.ROBModule.writeROB(77, idx, Waiting);
    cpu.curCPUstate = cpu.nextCPUstate;

    cpu.commit();

    ts = check_eq(ts, 0, cpu.nextCPUstate.reg[11].read(),
                  "commit: skip non-ready ROB");
  }

  // ========== Full Pipeline Cycle Tests ==========
  printf("\n=== Full Pipeline Cycle Tests ===\n");
  {
    Memory mem;
    CPU cpu(mem);

    init_all_RS(cpu.curCPUstate);
    int tag = cpu.curCPUstate.ROBModule.push(ROBEntry(REGISTER));

    cpu.curCPUstate.IntegerRS[0].free = false;
    cpu.curCPUstate.IntegerRS[0].op = ADD;
    cpu.curCPUstate.IntegerRS[0].vj = 10;
    cpu.curCPUstate.IntegerRS[0].vk = 5;
    cpu.curCPUstate.IntegerRS[0].qj = -1;
    cpu.curCPUstate.IntegerRS[0].qk = -1;
    cpu.curCPUstate.IntegerRS[0].ROB_dest = tag;
    cpu.nextCPUstate = cpu.curCPUstate;

    cpu.execute();

    ts = check(ts, !cpu.nextCPUstate.ALUModule.isEmpty(),
               "full cycle: execute produced result");

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
                  "full cycle: writeBack forwarded to waiting RS");
    ts = check_eq(ts, -1, cpu.nextCPUstate.IntegerRS[1].qj,
                  "full cycle: writeBack cleared qj");

    cpu.curCPUstate = cpu.nextCPUstate;
    int write_idx = cpu.curCPUstate.ROBModule.getIndex(tag);
    cpu.nextCPUstate.ROBModule.writeROB(15, write_idx, Ready);

    cpu.commit();

    ts = check_eq(ts, 15, cpu.nextCPUstate.reg[0].read(),
                  "full cycle: commit result");
  }

  // ========== Results ==========
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