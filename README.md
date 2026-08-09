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
├── src/                           # 模拟器源码（单体 Tomasulo 实现）
│   ├── main/main.cpp              # 入口
│   ├── CPU/CPU.cpp                # 流水线调度核心（read → fetch → issue → writeBack
│   │                              #   → execute → commit → flush → decode）
│   ├── include/                   # 全部头文件
│   │   ├── common.hpp             # 容量常量与公共结构体（ROBEntry / ExecuteResult / …）
│   │   ├── CPU.hpp                # CPU 状态与阶段声明
│   │   ├── ALU.hpp / Arbiter.hpp  # ALU 输出缓冲 / CDB 仲裁器
│   │   ├── BRU.hpp                # 分支执行单元（含输出缓冲）
│   │   ├── BranchPredictor.hpp    # Tournament 分支预测器（LHT + gshare + selector + BTB + RAS）
│   │   ├── Decoder.hpp            # 译码器
│   │   ├── INQ.hpp                # 指令队列
│   │   ├── LSQ.hpp                # 加载/存储队列（store→load 转发）
│   │   ├── Memory.hpp             # 带延迟的数据/指令内存
│   │   ├── Register.hpp           # 寄存器堆 + RAT
│   │   ├── ROB.hpp                # 重排序缓冲
│   │   ├── RS.hpp                 # 保留站（Integer / Load / Store / MicroStore / Branch）
│   │   └── util.hpp               # 调试宏（VERBOSE 主题开关）
│   ├── ALU/  BRU/  BranchPredictor/  Decoder/  INQ/  LSQ/  Memory/
│   └── Register/  ROB/
├── data/                          # 测试数据
│   ├── sample/                    # 示例程序
│   ├── testcases/                 # 18 个官方测试点（.c 源码 + .data 机器码 + .dump 反汇编）
│   └── golden/                    # 基线（返回值 + 时钟数）
├── test/                          # 乱序执行一致性测试（独立构建，不影响 OJ 提交）
│   ├── reorder_test.cpp           # 以任意顺序调用 7 个流水级的测试驱动
│   ├── CMakeLists.txt
│   └── test_reorder.sh            # 一致性测试脚本
├── test.sh                        # 代码行为测试脚本（返回值 / 分支正确率 / 时钟数）
├── ppt/                           # 讲义（lec1.pdf … lec4.pdf）
└── reference/                     # 参考资料（RISC-V 规范 / 指令卡 / 教材 PDF）
```


---

## 3. 执行流程

1. **读入机器指令**：从内存 `0x0000` 处开始取指，每次连取 **4 个 byte** 拼成一条指令。
2. 按 Tomasulo 流程（fetch → issue → exec → write & broadcast → commit）运行。
3. **终止与返回**：执行到指令 `0x0ff00513`（`li a0, 255`）时，**不要执行它**，向 `stdout`输出程序的返回值并停止。返回值 = `a0` 寄存器（即 `x10`）的**低 8 位**，是一个 `0–255`的非负整数。
4. **`x0` 每周期重置为 0**。

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

`test/reorder_test` 以**任意顺序**调用 7 个流水级（fetch / decode / issue / exec / writeBack / commit / flush），要求所有排列得到**完全相同**的返回值（`x10&0xFF`）与**完全相同**的时钟周期数，并与 `data/golden/` 基线对比。

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
| < 100ms | 全量 **5040** 组 |
| 100 ~ 10000ms | 随机 **100** 组 |
| > 10000ms | 仅参考序 **1** 组（`ref` 模式，如 pi） |

输出列：`Program`、`Count`（档位）、`Perms`（实际排列数）、`Value`（`x10&0xFF`）、`Clock`、`Golden(x10/clk)`、`Result`、`Time`。

判定规则：

- **OK** — `value` 与 `clock` 均与 golden 一致；
- **CLK** — `value` 一致但 `clock` 与 golden 不同（周期仅要求尽力对齐，不硬性要求）；
- **FAIL** — 功能值不一致，或不同乱序排列之间结果不一致。

`reorder_test` 也可直接使用：`./reorder_test all`（5040 组）、`./reorder_test <N>`（N 组随机）、`./reorder_test ref`（仅参考序 1 组），从 stdin 读入 `<test>.data`。

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

`VERBOSE` 支持逗号分隔的主题：`branch`、`clock`、`issue`、`exec`、`wb`、`commit`、`lsq`、`mem`，或 `all`。其中 `branch` 输出 `branch: <正确>/<总数> correct (<正确率>%)`，`clock` 输出 `clock: <时钟数>`。

---

## 6. 测试结果

**18 / 18 全部通过**。以下为 `./test.sh` 的完整输出（含时钟周期数与分支预测正确率），
单测试点耗时为本机（x86-64）实测墙钟时间。

### 6.1 分支预测器配置

Tournament 混合预测器：

| 部件 | 配置 |
|------|------|
| Local（LHT + PHT） | 4096 条目 LHT（每分支 8 位历史），256 个 2-bit 饱和计数器按历史模式索引 |
| Global（gshare） | 12 位全局历史 GHR，4096 条目 2-bit 饱和计数器，按 `PC ^ GHR` 索引 |
| 选择器 selector | 4096 个 2-bit 饱和计数器，仲裁 local / global |
| BTB | 4096 条目，预测 taken 时给出目标地址 |
| RAS | 128 条目返回地址栈（函数调用/返回预测） |

> 设计说明：LHT 在分支**解析时**用真实结果移位，天然非投机，squash 无需回滚（零快照）；
> 而 GHR / RAS 在 fetch 时投机更新，因此在 ROB 中保存 checkpoint 用于 flush 恢复。

### 6.2 逐测试点结果

| 测试点 | x10（返回值） | 时钟周期数 | 分支正确/总数 | 准确率 | 耗时 | 结果 |
|--------|:---:|-----------:|--------------:|-------:|-----:|:----:|
| array_test1 | 123 | 315 | 19/48 | 39.58% | 0.04s | OK |
| array_test2 | 43 | 338 | 28/54 | 51.85% | 0.03s | OK |
| basicopt1 | 88 | 674,092 | 168,586/199,513 | 84.50% | 2.31s | OK |
| bulgarian | 159 | 381,057 | 89,747/93,446 | 96.04% | 1.49s | OK |
| expr | 58 | 825 | 90/127 | 70.87% | 0.03s | OK |
| gcd | 178 | 842 | 98/185 | 52.97% | 0.03s | OK |
| hanoi | 20 | 232,608 | 26,960/30,327 | 88.90% | 0.91s | OK |
| lvalue2 | 175 | 99 | 8/18 | 44.44% | 0.03s | OK |
| magic | 106 | 794,658 | 90,232/103,476 | 87.20% | 3.18s | OK |
| manyarguments | 40 | 109 | 11/21 | 52.38% | 0.03s | OK |
| multiarray | 115 | 2,004 | 212/277 | 76.53% | 0.03s | OK |
| naive | 94 | 58 | 0/4 | 0.00% | 0.03s | OK |
| pi | 137 | 138,247,159 | 36,095,373/43,323,217 | 83.32% | 453.56s | OK |
| qsort | 105 | 1,308,377 | 266,372/273,578 | 97.37% | 4.75s | OK |
| queens | 171 | 930,263 | 84,825/104,914 | 80.85% | 3.41s | OK |
| statement_test | 50 | 1,585 | 180/293 | 61.43% | 0.03s | OK |
| superloop | 134 | 564,167 | 443,782/453,238 | 97.91% | 1.87s | OK |
| tak | 186 | 2,022,283 | 188,272/197,273 | 95.44% | 8.52s | OK |
| **合计** | — | **145,160,839** | **37,454,795/44,780,009** | **83.64%** | — | **18/18** |

> 说明：`naive` 只有 4 次条件分支且属于基本不可预测的模式，正确率 0% 属正常；
> `array_test1` 等小测试点分支基数小，正确率波动大，参考大测试点（qsort / superloop /
> bulgarian 均 ≥ 96%）为准。周期数只要求与 golden 尽力对齐，不硬性一致。

### 6.3 复现

```bash
cmake -S . -B build && cmake --build build   # 生成 ./code
./test.sh                                    # 全量测试（pi 约需 7~8 分钟）
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
