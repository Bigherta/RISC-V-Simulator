cmake_minimum_required(VERSION 3.10)
project(CPU_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/src/include/helper)
include_directories(${CMAKE_SOURCE_DIR}/src/include)

file(GLOB_RECURSE CPU_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(code ${CPU_SOURCES})

# 开发期开启模板的调试检测（Register 双写断言等）
target_compile_definitions(code PRIVATE _DEBUG)

set_target_properties(code PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
)