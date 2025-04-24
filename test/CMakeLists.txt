cmake_minimum_required(VERSION 3.20.0)

add_subdirectory(stl_util_ut)
add_subdirectory(gemm_validation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LOCAL_SRC_FILES
    ${ROOT_DIR}/src/thread_pool_bind.cpp
)

set(LOCAL_TEST_FILES
    ${ROOT_DIR}/test/main.cpp
    ${ROOT_DIR}/test/thread_pool_bind_ut.cpp
    ${ROOT_DIR}/test/thread_pool_ut.cpp
    ${ROOT_DIR}/test/lock_queue_ut.cpp
    ${ROOT_DIR}/test/lock_free_queue_ut.cpp
    ${ROOT_DIR}/test/fsm_ut.cpp
)

set(LOCAL_INC_DIRS
    ${ROOT_DIR}/src
    ${ROOT_DIR}/inc
)

add_executable(thread_pool_ut ${LOCAL_SRC_FILES} ${LOCAL_TEST_FILES})

target_link_libraries(thread_pool_ut gtest gcov)

target_compile_options(thread_pool_ut PRIVATE -g -std=c++17 -pthread -O3 -fPIC -fno-exceptions --coverage)

include_directories(thread_pool_ut ${LOCAL_INC_DIRS})

add_test(NAME thread_pool_ut COMMAND thread_pool_ut)

add_executable(avx_example avx_example.cpp)

target_compile_options(avx_example PRIVATE -g -std=c++17 -pthread -O3 -fPIC -fno-exceptions -mavx2 -mfma)
