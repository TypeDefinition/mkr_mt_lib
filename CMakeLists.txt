# CMake Version
cmake_minimum_required(VERSION 3.23.2)

# Project Name
project(mkr_mt_lib)

# C++ Version
set(CMAKE_CXX_STANDARD 20)

# Compiler Flags
set(CMAKE_CXX_FLAGS "-std=c++20 -O3")
# set(CMAKE_CXX_FLAGS "-std=c++20 -Og -fsanitize=thread")
# set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=thread")

# Source Files
set(SRC_DIR "src")
file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES true CONFIGURE_DEPENDS
        "${SRC_DIR}/*.h"
        "${SRC_DIR}/*.c"
        "${SRC_DIR}/*.hpp"
        "${SRC_DIR}/*.cpp")
add_library(${PROJECT_NAME} ${SRC_FILES})

# Multithreading Library
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)