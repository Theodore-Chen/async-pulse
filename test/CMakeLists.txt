cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

include(GoogleTest)

add_subdirectory(queue)
add_subdirectory(std_sandbox)
add_subdirectory(thread_pool)
add_subdirectory(fsm)
# add_subdirectory(gemm_validation)

