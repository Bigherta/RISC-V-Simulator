cmake_minimum_required(VERSION 3.20)
project(dark_template LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include/helper)
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "src/*.cpp")

add_executable(sim_tmpl main.cpp ${sources})

# 开发期开启模板的调试检测（Register 双写断言等）
target_compile_definitions(sim_tmpl PRIVATE _DEBUG)

# ALU-ROB 联调测试（连线 + 辅助函数）
add_executable(alu_rob_test test/alu_rob_test.cpp
    src/ALU.cpp src/ROB.cpp src/CDB_Arb.cpp)
target_compile_definitions(alu_rob_test PRIVATE _DEBUG)

# M3 发射链路联调测试（INQ -> Decoder -> RAT -> ROB push + REG 写回）
add_executable(inq_rat_regfile_test test/inq_rat_regfile_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp)
target_compile_definitions(inq_rat_regfile_test PRIVATE _DEBUG)

# M4a 整数执行路径联调测试（RAT issue -> INT_RS -> EXEC_SEL -> ALU -> CDB_Arb -> ROB/REG）
add_executable(rs_cdb_test test/rs_cdb_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp)
target_compile_definitions(rs_cdb_test PRIVATE _DEBUG)

# M4b-1 LOAD/BRANCH 路径联调测试（LOAD_RS/BRANCH_RS + 仲裁）
add_executable(rs_lb_test test/rs_lb_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/LOAD_RS.cpp src/BRANCH_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp)
target_compile_definitions(rs_lb_test PRIVATE _DEBUG)

# M4b-2 STORE 路径联调测试（STORE_RS 地址+操作数双槽、交付链）
add_executable(rs_store_test test/rs_store_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/STORE_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp)
target_compile_definitions(rs_store_test PRIVATE _DEBUG)

# M4b-3 LSQ 联调测试（push/地址写/交付转发/双派发/cdbDetect/出队/squash）
add_executable(rs_lsq_test test/rs_lsq_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/LOAD_RS.cpp src/STORE_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/LSQ.cpp)
target_compile_definitions(rs_lsq_test PRIVATE _DEBUG)

# M5 内存链联调测试（LSQ -> DMEM 派发/3 拍/回填/广播 + 在途阻塞 + 符号扩展）
add_executable(rs_mem_test test/rs_mem_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/LOAD_RS.cpp src/STORE_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/LSQ.cpp src/MEM.cpp)
target_compile_definitions(rs_mem_test PRIVATE _DEBUG)

# IMEM 取指 + 完整流水联调测试（共享 backing：fetch -> issue -> exec -> DMEM -> commit）
add_executable(imem_pipeline_test test/imem_pipeline_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/LOAD_RS.cpp src/STORE_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/LSQ.cpp src/MEM.cpp
    src/PC_Reg.cpp)
target_compile_definitions(imem_pipeline_test PRIVATE _DEBUG)

# M6a 分支执行联调测试（BRU：候选接收/比较/BRANCH_RS 释放/ROB bru 端口/squash flush）
add_executable(branch_test test/branch_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/BRANCH_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/BRU.cpp)
target_compile_definitions(branch_test PRIVATE _DEBUG)

# M6b squash 完整链联调测试（FLUSH_ARB 自动触发 + RAT repair + 深 squash 重仲裁 +
# ROB bru 端口 squash 条件 + JALR JumpSquash + 恢复）
add_executable(squash_test test/squash_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/BRANCH_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/BRU.cpp
    src/FLUSH_ARB.cpp src/MEM.cpp src/PC_Reg.cpp)
target_compile_definitions(squash_test PRIVATE _DEBUG)

# M6c BPU 联调测试（PC_Reg 独立模块 + BPU 预测/更新/recover + 冷热分支 + JAL/JALR RAS 往返）
add_executable(bpu_test test/bpu_test.cpp
    src/INQ.cpp src/Decoder.cpp src/RAT.cpp src/REG.cpp src/ROB.cpp
    src/INT_RS.cpp src/BRANCH_RS.cpp
    src/EXEC_SEL.cpp src/CDB_Arb.cpp src/ALU.cpp src/BRU.cpp
    src/FLUSH_ARB.cpp src/MEM.cpp src/PC_Reg.cpp src/BPU.cpp)
target_compile_definitions(bpu_test PRIVATE _DEBUG)
