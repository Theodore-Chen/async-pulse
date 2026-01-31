add_executable(queue_stress_test)

target_sources(queue_stress_test PRIVATE
    queue_stress_test.cpp
)

target_include_directories(queue_stress_test PRIVATE
    ${ROOT_DIR}/src
    ${ROOT_DIR}/test/unit/queue
)

target_compile_options(queue_stress_test PRIVATE
    -g
    -O3
    -fPIC
    -fno-exceptions
)

target_link_libraries(queue_stress_test PRIVATE
    hp
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(queue_stress_test)
