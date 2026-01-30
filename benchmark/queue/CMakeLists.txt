add_executable(queue_bench)

target_sources(queue_bench PRIVATE
    queue_bench.cpp
    ${ROOT_DIR}/src/hp/smr.cpp
)

target_include_directories(queue_bench PRIVATE ${ROOT_DIR}/src/)

target_link_libraries(queue_bench PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
)
