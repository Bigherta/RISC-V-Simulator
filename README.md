# PPCA 2026 · RISC-V Tomasulo CPU Simulator

本仓库是《计算机系统》课程大作业（PPCA 2026）的下发材料**用 C++ 模拟一个采用 Tomasulo 架构的 RV32I RISC-V CPU，通过所有下发数据**。

作业说明以 [`issue.pdf`](./issue.pdf) 为准（作业内容 / 评分标准 / 执行流程 / 下发文件 /各项要求）。本 README 对仓库结构、执行流程与关键注意事项做一个总览。

> ⚠️ 作业本身**严禁 AI 生成代码或自动补全**，需自行实现并勤于 `git commit`。

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
├── issue.pdf                 
├── reference/                
│   ├── reference-card.pdf               
│   ├── riscv-spec-20191213.pdf          
│   ├── RISC-V-Reader-Chinese-v2p1.pdf   
│   └── CAAQA5.pdf                        
├── ppt/                      
│   └── lec1.pdf … lec4.pdf
└── data/                     
    ├── sample/               
    └── testcases/            
```

---

## 3. 执行流程

1. **读入机器指令**：读入机器指令。从内存 `0x0000` 处开始取指，每次连取 **4 个 byte** 拼成一条指令。
2. 按 Tomasulo 流程（fetch → issue → exec → write & broadcast → commit）运行。
3. **终止与返回**：执行到指令 `0x0ff00513`（`li a0, 255`）时，**不要执行它**，向 `stdout`输出程序的返回值并停止。返回值 = `a0` 寄存器（即 `x10`）的**低 8 位**，是一个 `0–255`的非负整数。
4. **`x0` 每周期重置为 0**。

---

## 4. 下发数据格式

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

## 6. 评分标准

| 项目 | 占比 |
|------|------|
| 测试点 + 理解得分 | **75% + 15%** |
| 缺陷情况（未实现分支预测 / 主要功能缺陷） | **60% + 10%** |
| CR（代码审查） | **10%** |
| Bonus | **10%** |

> 理解得分在 Code Review 问答中评定；若实现有主要功能缺陷（如未实现分支预测），测试点与
> 理解占比会下调为 60% + 10%。

**Bonus 方向**：高级分支预测、Cache / 多发射（multiple issue）、V-Extension / SIMD...（如果已经写完基础内容，想要写 bonus 的同学请知会一声助教，对 bonus 内容有什么不理解的也欢迎大家积极来询问助教）

---

## 7. 实现建议与注意事项

来自 [`issue.pdf`](./issue.pdf)，并补充讲义要点：

- **模拟硬件思想**：一个元件只能通过「自己当前状态 + 外部输入」改变状态、产生输出；
  不得直接读全局变量或内存的瞬时值。
- **模块可交换**：大家写的 C++ 程序是顺序执行的；但实际硬件中元件是并行执行的，因此要求
  各 module 的执行顺序**可以任意交换**（CR 时会随机打乱顺序以验证时序逻辑是否正确）。
- 避免绝大多数 STL 容器 / 指针 / 引用 / 动态分配内存空间；**避免全局变量**。
- 建议各部件存储**新、旧两个状态**：执行时用自己和其他部件的旧状态运算出新状态，更新时
  再用新状态覆盖（模拟 reg 随时钟更新的时序逻辑）。
- 数据内存访问需模拟**硬件的延迟返回**，不得直接立即使用全局变量的值。
- 模拟器运行耗时短 ≠ 写得好；可在保证 Tomasulo 架构的前提下尝试减少时钟周期数
  （不会影响得分）。
- **建议先写一个单级流水的 naïve interpreter！** ① 熟悉 RISC-V 指令功能；② 方便 debug **对拍**——
  比较每 commit 一条指令后寄存器的状态。
- 想清楚每种指令都怎么处理之后再写，可以画一张设计图，体现各元件的功能、接线，以及元件间
  传输哪些信息（有「小巧思」拿不准的可找助教商议）。
- 写 decoder 要特别谨慎：务必弄清**符号扩展 / 无符号扩展**、要截取哪几位等特性；建议给
  decoder 写**单元测试**逐项验证。
- 很多「伪指令」其实是某些指令的特化形式，例如 `li rd,imm = addi rd,x0,imm`，
  `j = jal x0,offset`；提前知道这点，读 `.dump` 反汇编时就不会困惑。

---

## 8. 参考资料

- `reference/reference-card.pdf` —— 指令速查卡
- `reference/riscv-spec-20191213.pdf` —— RISC-V 官方规范（RV32I 精确定义）
- `reference/RISC-V-Reader-Chinese-v2p1.pdf` —— 《RISC-V 读者》中文版（拓展阅读 / bonus 参考）
- `reference/CAAQA5.pdf` —— 计算机组成与设计：硬件/软件接口（对应讲义「参考架构 CAAQA」）
- CAAQA 第三章：指令级并行（对应 `reference/CAAQA5.pdf`）
- [计算机体系结构-存储指令的加速 - 知乎](https://zhuanlan.zhihu.com/p/507619114)
- [同作者系列文章](https://www.zhihu.com/people/njugao-53/posts)
- [哔哩哔哩：第五到第九集](https://www.bilibili.com/video/av21376839/)

> 负责助教：王越天，陈柯杰
