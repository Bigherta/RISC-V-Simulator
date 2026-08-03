cmake_minimum_required(VERSION 3.10)
project(CPU_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPU_SOURCES
    src/CPU/CPU.cpp
    src/Decoder/Decoder.cpp
    src/Memory/Memory.cpp
    src/ROB/ROB.cpp
    src/ALU/ALU.cpp
    src/BRU/BRU.cpp
    src/LSQ/LSQ.cpp
    src/CDB/CDB.cpp
    src/Register/Register.cpp
    src/util/util.cpp
    src/INQ/INQ.cpp
)

add_executable(code
    src/main/main.cpp
    ${CPU_SOURCES}
)

target_include_directories(code PRIVATE src/include)