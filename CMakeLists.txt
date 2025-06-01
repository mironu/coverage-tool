cmake_minimum_required(VERSION 3.10)
project(CoverageTool)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(coverage_tool main.cpp)

target_link_libraries(coverage_tool
  clangTooling
  clangBasic
  clangASTMatchers
)
