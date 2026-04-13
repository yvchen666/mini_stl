cmake_minimum_required(VERSION 3.14)
project(phase02_iterator LANGUAGES CXX)

add_executable(test_iterator test_iterator.cpp)
target_include_directories(test_iterator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/phase01_allocator)
target_link_libraries(test_iterator PRIVATE GTest::gtest_main)
target_compile_options(test_iterator PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(test_iterator)
