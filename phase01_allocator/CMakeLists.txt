cmake_minimum_required(VERSION 3.14)
project(phase01_allocator LANGUAGES CXX)

add_executable(test_alloc test_alloc.cpp)
target_include_directories(test_alloc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_alloc PRIVATE GTest::gtest_main)
target_compile_options(test_alloc PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(test_alloc)
