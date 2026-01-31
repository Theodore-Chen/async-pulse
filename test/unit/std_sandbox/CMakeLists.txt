add_executable(std_sandbox)

target_sources(std_sandbox PRIVATE
    move_ut.cpp
    crtp_ut.cpp
    crtp_chainanle_ut.cpp
    shared_ptr_ut.cpp
    vector_ut.cpp
    map_ut.cpp
    unordered_map_ut.cpp
)

target_compile_options(std_sandbox PRIVATE -g -O3 -fPIC)

target_link_libraries(std_sandbox PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

gtest_discover_tests(std_sandbox)

