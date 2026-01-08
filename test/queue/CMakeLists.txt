set(CMAKE_CXX_SAVE_PREPROCESSED_FILES ON)

add_executable(queue_ut)

target_sources(queue_ut PRIVATE
    queue_ut.cpp
)

target_include_directories(queue_ut PUBLIC ${ROOT_DIR}/src/)

target_compile_options(queue_ut PRIVATE -fPIC -fno-exceptions)

target_link_libraries(queue_ut PRIVATE
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(queue_ut)
