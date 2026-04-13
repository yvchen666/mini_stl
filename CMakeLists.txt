cmake_minimum_required(VERSION 3.14)
project(mini_stl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建方式 (需要 g++-10):
#   CXX=g++-10 CC=gcc-10 cmake -B build -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build -j$(nproc)
#   ctest --test-dir build --output-on-failure

# ----- Google Test via FetchContent -----
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# 不安装 GTest 到系统
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(phase01_allocator)
add_subdirectory(phase02_iterator)
add_subdirectory(phase03_containers)
add_subdirectory(phase04_algorithm)
add_subdirectory(phase05_functional)
