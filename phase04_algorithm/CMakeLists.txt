cmake_minimum_required(VERSION 3.14)
project(phase04_algorithm LANGUAGES CXX)

add_executable(test_algorithm test_algorithm.cpp)
target_include_directories(test_algorithm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/phase01_allocator
    ${CMAKE_SOURCE_DIR}/phase02_iterator
    ${CMAKE_SOURCE_DIR}/phase03_containers)
target_link_libraries(test_algorithm PRIVATE GTest::gtest_main)
target_compile_options(test_algorithm PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(test_algorithm)
