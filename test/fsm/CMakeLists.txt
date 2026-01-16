add_executable(fsm_ut)

target_sources(fsm_ut PRIVATE
    fsm_ut.cpp
)

target_include_directories(fsm_ut PUBLIC ${ROOT_DIR}/src)

target_compile_options(fsm_ut PRIVATE -g -O3 -fPIC -fno-exceptions)

target_link_libraries(fsm_ut PRIVATE
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(fsm_ut)
