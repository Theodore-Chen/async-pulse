cmake_minimum_required(VERSION 3.18)
project(MatrixBenchmark LANGUAGES CXX CUDA) # 显式声明CUDA语言支持

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gemm_validation)

target_sources(gemm_validation PRIVATE
    main.cpp
    matrix.cpp
    avx2_gemm.cpp
)

target_compile_options(gemm_validation PRIVATE -g -std=c++17 -pthread -O3 -fPIC -mavx2 -mfma)

# openblas库调用支持
target_sources(gemm_validation PRIVATE openblas_gemm.cpp)

include_directories(/home/zqc/local/openblas/include/openblas)

target_link_directories(gemm_validation PRIVATE /home/zqc/local/openblas/lib)

target_link_libraries(gemm_validation PRIVATE openblas)

# CUDA调用支持
enable_language(CUDA)

target_sources(gemm_validation PRIVATE cublas_gemm.cpp)

include_directories(/usr/local/cuda-12.6/targets/x86_64-linux/include)

# 设置CUDA编译参数
target_compile_options(gemm_validation PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--default-stream per-thread>
    $<$<COMPILE_LANGUAGE:CUDA>:-O3>
)

target_link_directories(gemm_validation PRIVATE /usr/local/cuda-12.6/targets/x86_64-linux/lib)

target_link_libraries(gemm_validation PRIVATE cudart cublas)
