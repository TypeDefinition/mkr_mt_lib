# CMake Version
cmake_minimum_required(VERSION 3.17)

# Project Name
project("mkr_multithread_library")

# C++ Version
set(CMAKE_CXX_STANDARD 20)

# Source Directory
set(SRC_DIR "src")

# Build Type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    # set(CMAKE_BUILD_TYPE Debug)
endif()
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# Compiler Flags
set(CMAKE_CXX_FLAGS "-std=c++20 -O3")
# set(CMAKE_CXX_FLAGS "-std=c++20 -Og -fsanitize=thread")
# set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=thread")

# Source Files
file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES true CONFIGURE_DEPENDS
        "${SRC_DIR}/*.h"
        "${SRC_DIR}/*.c"
        "${SRC_DIR}/*.hpp"
        "${SRC_DIR}/*.cpp")
add_library("mkr_multithread_library" ${SRC_FILES})

# Multithreading Library
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries("mkr_multithread_library" Threads::Threads)
