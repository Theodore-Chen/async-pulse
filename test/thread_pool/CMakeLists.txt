add_library(thread_pool_ut_lib STATIC ${ROOT_DIR}/src/thread_pool/thread_pool_bind.cpp)

add_executable(thread_pool_ut)

target_sources(thread_pool_ut PRIVATE
    thread_pool_bind_ut.cpp
    thread_pool_ut.cpp
)

target_include_directories(thread_pool_ut PUBLIC ${ROOT_DIR}/src)

target_compile_options(thread_pool_ut PRIVATE -g -O3 -fPIC -fno-exceptions)

target_link_libraries(thread_pool_ut PRIVATE
    thread_pool_ut_lib
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(thread_pool_ut)

