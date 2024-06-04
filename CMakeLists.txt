cmake_minimum_required(VERSION 3.19)
project(gocoroutine)

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fcoroutines")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
set(EXECUTABLE_OUTPUT_PATH "CMakeFiles/build/")

include_directories("include" "src/include")

add_executable("test_task" "test/test_task.cc")

add_executable("test_executor" "test/test_executor.cc")

add_executable("test_scheduler" "test/test_scheduler.cc")