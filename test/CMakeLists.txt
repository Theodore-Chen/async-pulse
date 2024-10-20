cmake_minimum_required(VERSION 3.8.0)

set(LOCAL_SRC_FILES
    ${ROOT_DIR}/src/thread_pool_bind.cpp
)

set(LOCAL_TEST_FILES
    ${ROOT_DIR}/test/main.cpp
    ${ROOT_DIR}/test/thread_pool_bind_ut.cpp
    ${ROOT_DIR}/test/thread_pool_ut.cpp
    ${ROOT_DIR}/test/lock_queue_ut.cpp
)

set(LOCAL_INC_DIRS
    ${ROOT_DIR}/src
    ${ROOT_DIR}/inc
)

add_executable(thread_pool_ut ${LOCAL_SRC_FILES} ${LOCAL_TEST_FILES})

target_link_libraries(thread_pool_ut gtest gcov)

target_compile_options(thread_pool_ut PRIVATE -g -std=c++11 -pthread -Os -fPIC --coverage)

include_directories(thread_pool_ut ${LOCAL_INC_DIRS})

add_test(NAME thread_pool_ut COMMAND thread_pool_ut)