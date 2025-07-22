add_executable(queue_ut)

target_sources(queue_ut PRIVATE
    # lock_free_queue_ut.cpp
    lock_queue_ut.cpp
)

target_include_directories(queue_ut PUBLIC ${ROOT_DIR}/src)

target_compile_options(queue_ut PRIVATE -g -O3 -fPIC -fno-exceptions)

target_link_libraries(queue_ut PRIVATE
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(queue_ut)

