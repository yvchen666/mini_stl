cmake_minimum_required(VERSION 3.14)
project(phase05_functional LANGUAGES CXX)

add_executable(test_functional test_functional.cpp)
target_include_directories(test_functional PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/phase01_allocator
    ${CMAKE_SOURCE_DIR}/phase02_iterator
    ${CMAKE_SOURCE_DIR}/phase03_containers)
target_link_libraries(test_functional PRIVATE GTest::gtest_main)
target_compile_options(test_functional PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(test_functional)
