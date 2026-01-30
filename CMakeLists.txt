cmake_minimum_required(VERSION 3.20.0)

project(AsyncPulse VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

enable_testing()

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(BUILD_TESTS "Build test executables (requires Google Test)" ON)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

option(BUILD_BENCHMARKS "Build benchmark executables (requires Google Benchmark)" ON)

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
