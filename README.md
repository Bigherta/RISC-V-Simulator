# PPCA 2026 · RISC-V Tomasulo CPU Simulator

本仓库是《计算机系统》课程大作业（PPCA 2026）的下发材料**用 C++ 模拟一个采用 Tomasulo 架构的 RV32I RISC-V CPU，通过所有下发数据**。

作业说明以 [`issue.pdf`](./issue.pdf) 为准（作业内容 / 评分标准 / 执行流程 / 下发文件 /各项要求）。本 README 对仓库结构、执行流程与关键注意事项做一个总览。

---

## 1. 作业目标

- 实现**乱序执行（out-of-order）** 的 Tomasulo 算法：指令可以不等前序指令执行完，只要操作数就绪就执行；但**取指与提交仍然是顺序的**。
- 指令集为 **RV32I**（可参考 `reference/reference-card.pdf`，实现除 `ebreak`、`ecall`之外的指令）。
- 需要实现**分支预测**并统计预测准确率。
- 访存指令需模拟**硬件延迟返回**（不能直接立即使用内存/全局变量的瞬时值）。
- 统计花费的**时钟周期数**。
- 数据内存与指令内存的读写可以**同时进行**。

---

## 2. 仓库结构

```
RISC-V-Tomasulo-CPU-Simulator/
├── CMakeLists.txt                 # 根目录构建，生成 ./code
├── issue.pdf                      # 作业说明（评分标准 / 执行流程 / 下发数据）
├── README.md
├── src/                           # 模拟器源码（单体 Tomasulo 实现，逐周期快照双缓冲）
│   ├── main/main.cpp              # 入口
│   ├── CPU/CPU.cpp                # comb() 快照 + 组合求值（FetchDecision / CDBArbiter /
│   │                              #   CDBBus / IssueArbiter / DispatchArbiter /
│   │                              #   MemDispatchDecision / LoadResponse / LineReturn）
│   │                              #   → 按流水顺序调用 17 个模块的 tick
│   ├── include/                   # 全部头文件
│   │   ├── common.hpp             # 容量常量与公共结构体（ROBEntry / CDBBus / FetchDecision / …）
│   │   ├── CPU.hpp                # systemState（活体模块）+ CPU（快照成员 + Input 接线）
│   │   ├── AGU.hpp / ALU.hpp      # 地址计算 / 算术逻辑执行单元（含输出缓冲）
│   │   ├── Arbiter.hpp            # FlushArbiter（squash 仲裁）+ CDB / Dispatch 组合求值
│   │   ├── BRU.hpp                # 分支执行单元（含输出缓冲）
│   │   ├── BranchPredictor.hpp    # Tournament 分支预测器（LHT + gshare + selector + BTB + RAS）
│   │   ├── Decoder.hpp            # 译码器 + 指令队列 IQ
│   │   ├── DMEM.hpp / IMEM.hpp    # 数据 / 指令内存（IMEM 带 3 周期延迟管线与整行突发）
│   │   ├── FetchQueue.hpp         # 取指队列 FQ
│   │   ├── FetchUnit.hpp          # 前端 PC 寄存器 + halt 状态（程序计数器 / haltFetched）
│   │   ├── ICache.hpp             # 16KB 直接映射指令缓存（1024 行 × 16B，word4 行总线）
│   │   ├── IssueArbiter.hpp       # issue 组合分配（IssuePacket，无 tick）
│   │   ├── LSQ.hpp                # 加载/存储队列（store→load 转发）
│   │   ├── Memory.hpp             # 带延迟的内存基类
│   │   ├── PRF.hpp                # 物理寄存器堆（rename / free list / 完成端口）
│   │   ├── RAT.hpp                # RAT（架构寄存器→物理寄存器映射表）
│   │   ├── ROB.hpp                # 重排序缓冲（条目含 checkpoint 快照）
│   │   ├── RS.hpp                 # 保留站（Integer / Load / StoreAddress / StoreValue / Branch）
│   │   └── util.hpp               # 调试宏（VERBOSE 主题开关）
│   ├── AGU/  ALU/  Arbiter/  BRU/  BranchPredictor/  CPU/  DMEM/  Decoder/
│   ├── FetchQueue/  FetchUnit/  ICache/  IMEM/  IssueArbiter/  LSQ/  PRF/
│   ├── RAT/  ROB/  RS/   # 各模块 tick 实现
│   └── main/
├── data/                          # 测试数据
│   ├── sample/                    # 示例程序
│   ├── testcases/                 # 18 个官方测试点（.c 源码 + .data 机器码 + .dump 反汇编）
│   └── golden/                    # 基线（返回值 + 时钟数）
├── test/                          # 乱序执行一致性测试（独立构建，不影响 OJ 提交）
│   ├── reorder_test.cpp           # 以任意顺序调用 17 个流水级的测试驱动
│   ├── CMakeLists.txt
│   └── test_reorder.sh            # 一致性测试脚本
├── test.sh                        # 代码行为测试脚本（返回值 / 分支正确率 / 时钟数）
├── ppt/                           # 讲义（lec1.pdf … lec4.pdf）
└── reference/                     # 参考资料（RISC-V 规范 / 指令卡 / 教材 PDF）
```


---

## 3. 执行流程

每个周期按以下顺序推进（所有阶段只读周期开头的**快照**，只写自己的活体状态，
故阶段调用顺序任意交换结果不变，见 §5.1）：

1. **comb()**：将全部模块 memcpy 进快照，并在快照边组合求值：`FetchDecision`
   （取指决策）、`CDBArbiter` / `CDBBus`（写回广播）、`IssueArbiter::build`
   （issue 分配）、`DispatchArbiter`（执行单元派发）、`selectMemRequest`
   （LSQ → DMEM 请求准入决策，store 优先互斥）、`DMEM::LoadReturn`（DMEM → LSQ
   load 回复总线）。所有跨模块总线信号在 comb 阶段打包装入对应 `Input`；各模块
   tick 沿采样、写自己，**跨模块写为零**。
2. **取指**：`FetchUnit.tick` 管理 PC / halt 状态（squash 恢复 PC、ICache 头部出现
   halt 时 latch haltFetched、fetchDecision 有效时推进 PC）——`IMEM.tick` 三段式
   （消费返回 / 认领行请求 / 管线递减），带 3 周期延迟的整行突发；命中路径由
   `ICache.tick` 直接命中组包入队，缺失路径经 IMEM 整行回填，取指结果经
   `FQ.tick` 压入取指队列（FQ 满则背压停取）。
3. **译码**：`DecodeUnit.tick` 从 FQ 头取指令译码入 IQ；FQ 头部消费由 FQ 自己
   （读 DecodeUnit 快照判空槽）完成。
4. **issue**：`IssueArbiter` 组合构建 `IssuePacket`（ROB/RS/LSQ 容量门控 + 操作数
   解析 + rename），六个模块的 tick 各自 apply（RAT 改名 / PRF pop+LINK / ROB push /
   RS 槽位 / LSQ push / Decode pop）；LSQ→DMEM 请求准入由 comb 边 `selectMemRequest`
   单次求值（store 优先互斥，`lsqInput.decision` = `dmemInput.decision`），`DMEM.tick`
   段1 直接认领（`!busy && decision.valid` → 写自己 `MemExecution` + `busy`）——
   `busy` 仅由 DMEM 置位，"至多一条在飞"从生产者信任升级为消费者强制。
5. **执行与写回**：ALU/AGU/BRU 的操作数就绪即乱序执行；结果经 `CDBBus` 广播
   （保留站 / PRF / ROB-ready / LSQ 完成端口 / JALR 解析）。LSQ 完成的 load 回复由
   comb 边 `DMEM::LoadReturn` 组合填入 `lsqInput.loadResp`，`LSQ.tick` 沿采样
   写自己 `value`/READY（**DMEM 自己清 `bufferValid`**，comb 纯读）；返回路径
   保持 2 拍 load-to-use。
6. **提交**：ROB 头就绪即按序提交（PRF 释放旧物理寄存器）；`0x0ff00513`（li a0, 255）
   照常取指入队，其后返回的指令直接丢弃（不译码不执行），提交到 halt 时停机。
7. **误预测恢复**：BRU/JALR 误预测经 `FlushArbiter` 仲裁（最老优先），各模块在自己
   tick 内恢复——RAT/PRF 从 ROB 条目的 checkpoint 快照恢复，GHR/RAS 由 BP 恢复，
   FQ/IQ/RS/LSQ 按快照回卷。
8. **终止与返回**：停机后向 `stdout` 输出 `x10`（架构寄存器 a0）的**低 8 位**（0–255）。
   `x0` 恒为 0，不参与 rename。

---

## 4. 数据格式

每个测试程序都包含**三个同名文件**：

| 文件 | 含义 | 用途 |
|------|------|------|
| `.c`   | 人类可读的 C 源码 | 理解程序在算什么（如求 pi、gcd、快排…） |
| `.data`| 机器码的**文本十六进制**表示 | **你的 simulator 的实际输入** |
| `.dump`| `objdump` 反汇编 | 便于对照每条机器码对应的汇编指令 |

### `.data` 格式

以 `@` 开头的行给出一个**加载地址**（十六进制），其后的若干行是**空格分隔的十六进制字节**，依次写入从该地址开始的连续内存。例如：

```
@00000000
37 01 02 00 EF 10 00 04 13 05 F0 0F B7 06 03 00
23 82 A6 00 6F F0 9F FF
@00001000
37 17 00 00 83 27 C7 06 ...
```

表示：从 `0x00000000` 写入 `37 01 02 00 …`，从 `0x00001000` 写入后续字节。

---

## 5. 验证脚本

仓库提供两个辅助脚本，分别验证**乱序执行的一致性**（CR 会随机打乱模块调用顺序，要求结果与周期数不变）与**代码行为**（返回值正确性、分支预测正确率、时钟周期数）。

### 5.1 乱序执行一致性测试 — `test/test_reorder.sh`

`test/reorder_test` 以**任意顺序**调用 17 个流水级（rat / lsq / decode / agu / bru / bp / dmem / alu / rs / rob / prf / arb / imem / fq / icache / fetchunit），要求所有排列得到**完全相同**的返回值（`x10&0xFF`）与**完全相同**的时钟周期数，并与 `data/golden/` 基线对比。

构建（独立于根目录，不影响 OJ 提交）：

```bash
cd test
cmake -S . -B build && cmake --build build     # 生成 test/reorder_test
```

运行（`reorder_test` 不存在时脚本会自动构建）：

```bash
cd test
./test_reorder.sh                 # 全部测试
./test_reorder.sh 'gcd'           # 只跑 gcd
./test_reorder.sh 'q*'            # 通配符过滤（如 qsort、queens）
./test_reorder.sh --count N       # 强制所有测试用 N 组随机乱序（调试用）
```

按每个测试点**单次运行耗时**（内置 `TIMING_MS` 表，来自评测数据）自动分档：

| 单次耗时 | 乱序组数 |
|---|---|
| < 100ms | 随机 **100** 组 |
| 100 ~ 10000ms | 随机 **100** 组 |
| > 10000ms | 仅参考序 **1** 组（`ref` 模式，如 pi） |

> 注：流水级已从早期的 7 阶段扩展为 17 阶段，全排列 = 17! = 355687428096000 组已不现实，
> 故快档统一用随机 100 组。

输出列：`Program`、`Count`（档位）、`Perms`（实际排列数）、`Value`（`x10&0xFF`）、`Clock`、`Golden(x10/clk)`、`Result`、`Time`。

判定规则：

- **OK** — `value` 与 `clock` 均与 golden 一致；
- **CLK** — `value` 一致但 `clock` 与 golden 不同（周期仅要求尽力对齐，不硬性要求）；
- **FAIL** — 功能值不一致，或不同乱序排列之间结果不一致。

`reorder_test` 也可直接使用（从 stdin 读入 `<test>.data`）：

```bash
./reorder_test 20  < data/testcases/gcd.data      # 20 组随机乱序
./reorder_test ref < data/testcases/pi.data       # 仅参考序 1 组
./reorder_test diff "rat,lsq,decode,agu,bru,bp,dmem,alu,rs,rob,prf,arb,imem,fq,icache,fetchunit" \
                  "fetchunit,icache,fq,imem,arb,prf,rob,rs,alu,dmem,bp,bru,agu,decode,sq,lq,rat" \
                  < data/testcases/gcd.data        # 两种指定顺序逐周期状态对比
```

`diff` 模式对比两种显式指定的阶段顺序，逐周期打印首个状态分叉点（cycle / 字段 / 值），
用于定位乱序不一致的根源（`all` = 全排列 17! 组，仅作参考，实际不可跑完）。

### 5.2 代码行为测试 — `test.sh`

以官方 `data/testcases/*.data` 作为输入运行根目录 `code`，同时验证**行为正确性**（返回值 `x10 & 0xFF` 与 `data/golden` 比对、崩溃检测）、**分支预测正确率**与**时钟周期数**。

先编译根目录 `code`：

```bash
cmake -S . -B build && cmake --build build     # 生成 ./code
```

运行：

```bash
./test.sh                  # 全部测试
./test.sh 'q*'             # 通配符过滤（如 qsort、queens）
BP_BIN=./code ./test.sh    # 指定可执行文件（默认 ./code）
```

输出列：`Program`、`Exit`（退出码，非 0 表示崩溃）、`Correct/Total`（分支预测正确数/总数）、`Accuracy`、`Clock`（总时钟周期数）、`Time`（单测试点墙钟耗时）、`x10`（返回值）、`Golden`（OK / FAIL / CRASH），末尾汇总 `TOTAL` 整体正确率与总时钟数。任一测试点 FAIL 或 CRASH 时脚本以非 0 退出（可用于 CI）。

#### 单独统计时钟数与分支预测正确率

不比对 golden，直接运行模拟器并输出统计（`branch:` 行与 `clock:` 行走 **stderr**）：

```bash
# 单个测试点
VERBOSE=branch,clock ./code < data/testcases/gcd.data

# 全部测试点
for f in data/testcases/*.data; do
  echo "== $(basename "$f" .data) =="
  VERBOSE=branch,clock ./code < "$f"
done
```

`VERBOSE` 支持逗号分隔的主题：`branch`、`clock`、`issue`、`exec`、`wb`、`commit`、`lsq`、`mem`、`prf`，或 `all`。其中 `branch` 输出 `branch: <正确>/<总数> correct (<正确率>%)`，`clock` 输出 `clock: <时钟数>`。

---

## 6. 测试结果

**18 / 18 全部通过**。以下为 `./test.sh` 的完整输出（含时钟周期数与分支预测正确率），
单测试点耗时为本机（x86-64）实测墙钟时间。

### 6.1 分支预测器配置

Tournament 混合预测器（方向预测与目标预测**按控制流类型拆分**，RAS 启用 `times` 合并）：

| 部件 | 配置 |
|------|------|
| Local（LHT + PHT） | 4096 条目 LHT（每分支 8 位历史），256 个 2-bit 饱和计数器按历史模式索引 |
| Global（gshare） | 12 位全局历史 GHR，4096 条目 2-bit 饱和计数器，按 `PC ^ GHR` 索引 |
| 选择器 selector | 4096 个 2-bit 饱和计数器，仲裁 local / global |
| BTB | 512 条目，预测 taken 时给出目标地址 |
| RAS | 16 条目 `RASEntry{retPC,times}`，同返回地址递归共用一条目（times 计数） |
| SARAS | 16 条目 `AlignQueue{addr,index,times}` + checkpoint 存 `GHR/alignHead/alignTail/RAS_top`（`BPUSnapshot` 5B，`CKPT_CAP=64`，`uint8` 环绕） |
| direction-split | **B 类条件分支**：方向表（LHT/gshare/selector）+ BTB；**JAL/JALR**：只更新 BTB（`unconditional` 恒 taken），方向表永不被恒跳指令污染 |

> 设计说明：训练在 `BPUpdateArbiter` 单点原子完成（BRU 候选 → CDB 候选固定序，
> 任意阶段乱序等价）；GHR 在**发请求时**随预测移位、误预测时由 ROB 条目的
> checkpoint 快照恢复（`recoverCheckPoint`，含 `RAS_top`）；RAS 随 call/ret 预测 push/pop 维护
> （dedup 时 `times++`、RET 时 `times--` 或 pop，`AlignQueue` 记录 `{addr,index,times}` 供 flush 撤销；
> 栈顶 peek 已禁用——错误路径污染，见 `docs/硬件行为差异.md` §3.27）。

### 6.2 逐测试点结果

| 测试点 | x10（返回值） | 时钟周期数 | 分支正确/总数 | 准确率 | 耗时 | 结果 |
|--------|:---:|-----------:|--------------:|-------:|-----:|:----:|
| array_test1 | 123 | 362 | 22/47 | 46.81% | 0.03s | OK |
| array_test2 | 43 | 373 | 31/53 | 58.49% | 0.03s | OK |
| basicopt1 | 88 | 634,754 | 180,078/199,517 | 90.26% | 2.01s | OK |
| bulgarian | 159 | 373,258 | 89,849/93,923 | 95.66% | 1.47s | OK |
| expr | 58 | 890 | 95/133 | 71.43% | 0.03s | OK |
| gcd | 178 | 925 | 115/188 | 61.17% | 0.03s | OK |
| hanoi | 20 | 217,966 | 25,837/29,238 | 88.37% | 0.86s | OK |
| lvalue2 | 175 | 141 | 8/18 | 44.44% | 0.03s | OK |
| magic | 106 | 789,993 | 92,006/105,852 | 86.92% | 3.04s | OK |
| manyarguments | 40 | 149 | 10/20 | 50.00% | 0.03s | OK |
| multiarray | 115 | 1,849 | 229/279 | 82.08% | 0.03s | OK |
| naive | 94 | 73 | 0/4 | 0.00% | 0.02s | OK |
| pi | 137 | 142,123,832 | 36,165,143/42,975,114 | 84.15% | 489.82s | OK |
| qsort | 105 | 1,332,296 | 267,385/276,586 | 96.67% | 4.42s | OK |
| queens | 171 | 892,496 | 85,915/106,237 | 80.87% | 3.37s | OK |
| statement_test | 50 | 1,697 | 181/286 | 63.29% | 0.03s | OK |
| superloop | 134 | 576,952 | 443,893/453,799 | 97.82% | 1.69s | OK |
| tak | 186 | 1,961,070 | 201,778/222,949 | 90.50% | 8.22s | OK |
| **TOTAL**| — | **148,909,075** | **37,552,575/44,464,243** | **84.46%** | — | **18/18** |

> 说明：`naive` 只有 4 次条件分支且属于基本不可预测的模式，正确率 0% 属正常；
> `array_test1` 等小测试点分支基数小，正确率波动大，参考大测试点（qsort / superloop
> 均 ≥ 96%）为准。周期数只要求与 golden 尽力对齐，不硬性一致。


### 6.3 复现

```bash
cmake -S . -B build && cmake --build build   # 生成 ./code
./test.sh                                    # 全量测试（pi 约 8 分钟）
```

---

## 7. 参考资料

- `reference/reference-card.pdf` —— 指令速查卡
- `reference/riscv-spec-20191213.pdf` —— RISC-V 官方规范（RV32I 精确定义）
- `reference/RISC-V-Reader-Chinese-v2p1.pdf` —— 《RISC-V 读者》中文版（拓展阅读 / bonus 参考）
- `reference/CAAQA5.pdf` —— 计算机组成与设计：硬件/软件接口（对应讲义「参考架构 CAAQA」）
- CAAQA 第三章：指令级并行（对应 `reference/CAAQA5.pdf`）
- [计算机体系结构-存储指令的加速 - 知乎](https://zhuanlan.zhihu.com/p/507619114)
- [同作者系列文章](https://www.zhihu.com/people/njugao-53/posts)
- [哔哩哔哩：第五到第九集](https://www.bilibili.com/video/av21376839/)
