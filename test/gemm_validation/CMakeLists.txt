project(MatrixBenchmark LANGUAGES CXX)

add_executable(gemm_validation)

target_sources(gemm_validation PRIVATE
    main.cpp
    matrix.cpp
    avx2_gemm.cpp
)

target_compile_options(gemm_validation PRIVATE -g -std=c++17 -pthread -O3 -fPIC -mavx2 -mfma)

find_package(BLAS QUIET)

if(BLAS_FOUND)
    target_sources(gemm_validation PRIVATE openblas_gemm.cpp)

    target_link_libraries(gemm_validation PRIVATE BLAS::BLAS)

    target_compile_definitions(gemm_validation PRIVATE HAS_OPENBLAS)
endif()

find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    enable_language(CUDA)

    target_sources(gemm_validation PRIVATE cublas_gemm.cpp)

    target_link_libraries(gemm_validation PRIVATE CUDA::cublas CUDA::cudart)

    target_compile_options(gemm_validation PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--default-stream per-thread -O3>
    )

    target_compile_definitions(gemm_validation PRIVATE HAS_CUDA)
endif()

