cmake_minimum_required(VERSION 3.16)
project(custom_memory_pool_allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(memory_pool_lib
    src/memory_pool.cpp
    src/memory_provider.cpp
    src/fixed_size_pool.cpp
    src/variable_size_pool.cpp
    src/first_fit.cpp
    src/best_fit.cpp
    src/benchmark.cpp
    src/experiment_metrics.cpp
    src/experiment_store.cpp
)

target_include_directories(memory_pool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(memory_pool_demo src/main.cpp)
target_link_libraries(memory_pool_demo PRIVATE memory_pool_lib)

add_executable(memory_pool_server src/server_main.cpp src/api_server.cpp)
target_link_libraries(memory_pool_server PRIVATE memory_pool_lib)

add_executable(allocator_tests tests/allocator_tests.cpp)
target_link_libraries(allocator_tests PRIVATE memory_pool_lib)

add_test(NAME allocator_tests COMMAND allocator_tests)
