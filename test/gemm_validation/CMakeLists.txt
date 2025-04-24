cmake_minimum_required(VERSION 3.18)
project(MatrixBenchmark LANGUAGES CXX CUDA) # 显式声明CUDA语言支持

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")

# 设置包含目录
# include_directories(include)

# 添加可执行文件
add_executable(gemm_validation
    main.cpp
    avx2_gemm.cpp
)

target_compile_options(gemm_validation PRIVATE -g -std=c++17 -pthread -O3 -fPIC -mavx2 -mfma)

# # 单独设置CUDA编译选项
# set_source_files_properties(src/cuda_part.cu PROPERTIES
#     LANGUAGE CUDA
#     CUDA_ARCHITECTURES "sm_86" # RTX 3080 Ti的计算能力
# )
# 
# # 设置CUDA编译参数
# target_compile_options(gemm_validation PRIVATE
#     $<$<COMPILE_LANGUAGE:CUDA>:--default-stream per-thread>
#     $<$<COMPILE_LANGUAGE:CUDA>:-O3>
# )
# 
# # 链接CUDA库
# target_link_libraries(gemm_validation PRIVATE
#     cublas cudart
# )
# 
# # 打印配置信息
# message(STATUS "CUDA found: ${CUDA_VERSION}")
# message(STATUS "CUDA toolkit directory: ${CUDA_TOOLKIT_ROOT_DIR}")
