cmake_minimum_required(VERSION 3.10)
project(CPU_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code
    src/main/main.cpp
    src/CPU/CPU.cpp
    src/Decoder/Decoder.cpp
    src/Memory/Memory.cpp
    src/ROB/ROB.cpp
)

target_include_directories(code PRIVATE src/include)

set_target_properties(code PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(cpu_test
    src/CPU/CPU_test.cpp
    src/CPU/CPU.cpp
    src/Decoder/Decoder.cpp
    src/Memory/Memory.cpp
    src/ROB/ROB.cpp
)

target_include_directories(cpu_test PRIVATE src/include)

set_target_properties(cpu_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")