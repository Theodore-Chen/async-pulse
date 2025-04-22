cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STL_UTIL_UT_DIR ${ROOT_DIR}/test/stl_util_ut)

set(LOCAL_TEST_FILES
    ${STL_UTIL_UT_DIR}/main.cpp
    ${STL_UTIL_UT_DIR}/move_ut.cpp
    ${STL_UTIL_UT_DIR}/crtp_ut.cpp
    ${STL_UTIL_UT_DIR}/crtp_chainanle_ut.cpp
    ${STL_UTIL_UT_DIR}/shared_ptr_ut.cpp
    ${STL_UTIL_UT_DIR}/vector_ut.cpp
    ${STL_UTIL_UT_DIR}/map_ut.cpp
    ${STL_UTIL_UT_DIR}/unordered_map_ut.cpp
)

add_executable(stl_util_ut ${LOCAL_TEST_FILES})

target_link_libraries(stl_util_ut gtest gmock gcov)

target_compile_options(stl_util_ut PRIVATE -g -std=c++17 -pthread -O3 -fPIC)

add_test(NAME stl_util_ut COMMAND stl_util_ut)
