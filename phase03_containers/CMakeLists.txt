cmake_minimum_required(VERSION 3.14)
project(phase03_containers LANGUAGES CXX)

set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/phase01_allocator
    ${CMAKE_SOURCE_DIR}/phase02_iterator)

foreach(target
    test_vector
    test_list
    test_deque
    test_stack_queue
    test_heap_priority_queue
    test_rb_tree
    test_map_set
    test_hashtable)
    add_executable(${target} ${target}.cpp)
    target_include_directories(${target} PRIVATE ${INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE GTest::gtest_main)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
endforeach()

include(GoogleTest)
foreach(target
    test_vector
    test_list
    test_deque
    test_stack_queue
    test_heap_priority_queue
    test_rb_tree
    test_map_set
    test_hashtable)
    gtest_discover_tests(${target})
endforeach()
