set(CMAKE_CXX_SAVE_PREPROCESSED_FILES ON)

add_library(hp STATIC)

target_sources(hp PRIVATE
    ${ROOT_DIR}/src/hp/smr.cpp
)

target_include_directories(hp PUBLIC ${ROOT_DIR}/src/)

target_compile_options(hp PRIVATE -fPIC -fno-exceptions)

add_executable(queue_ut)

target_sources(queue_ut PRIVATE
    bounded_queue_ut.cpp
    queue_ut.cpp
)

target_include_directories(queue_ut PRIVATE ${ROOT_DIR}/src ${ROOT_DIR}/test)

target_compile_options(queue_ut PRIVATE -fPIC -fno-exceptions)

target_link_libraries(queue_ut PRIVATE
    hp
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(queue_ut)
