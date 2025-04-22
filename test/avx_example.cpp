#include <immintrin.h>
#include <unistd.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <cmath>

// 普通标量版本矩阵加法
// __attribute__((optimize("no-tree-vectorize")))
// __attribute__((optimize("O0")))
void matrix_add_scalar(float* A, float* B, float* C, int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            C[i * cols + j] = A[i * cols + j] * B[i * cols + j] + B[i * cols + j] * B[i * cols + j];
        }
    }
}

// AVX2向量化版本矩阵加法
void matrix_add_avx256(float* A, float* B, float* C, int rows, int cols) {
    const int vec_size = 8;  // AVX2一次处理8个float
    for (int i = 0; i < rows; ++i) {
        int j = 0;
        // 主循环处理对齐部分
        for (; j <= cols - vec_size; j += vec_size) {
            __m256 a = _mm256_load_ps(&A[i * cols + j]);
            __m256 b = _mm256_load_ps(&B[i * cols + j]);
            __m256 c = _mm256_add_ps(_mm256_mul_ps(a, b), _mm256_mul_ps(b, b));
            _mm256_store_ps(&C[i * cols + j], c);
        }
        // 处理剩余不足8个的元素
        for (; j < cols; ++j) {
            C[i * cols + j] = A[i * cols + j] * B[i * cols + j] + B[i * cols + j] * B[i * cols + j];
        }
    }
}

// // AVX-512向量化版本矩阵加法
// void matrix_add_avx512(float* A, float* B, float* C, int rows, int cols) {
//     const int vec_size = 16;  // AVX-512一次处理16个float
//     for (int i = 0; i < rows; ++i) {
//         int j = 0;
//         // 主循环处理对齐部分
//         for (; j <= cols - vec_size; j += vec_size) {
//             __m512 a = _mm512_load_ps(&A[i * cols + j]);
//             __m512 b = _mm512_load_ps(&B[i * cols + j]);
//             __m512 c = _mm512_add_ps(_mm512_mul_ps(a, b), _mm512_mul_ps(b, b));
//             _mm512_store_ps(&C[i * cols + j], c);
//         }
//         // 处理剩余不足16个的元素
//         for (; j < cols; ++j) {
//             C[i * cols + j] = A[i * cols + j] * B[i * cols + j] + B[i * cols + j] * B[i * cols + j];
//         }
//     }
// }

// 验证结果正确性
bool verify_result(float* A, float* B, float* C, int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            float expected = A[i * cols + j] * B[i * cols + j] + B[i * cols + j] * B[i * cols + j];
            if (std::fabs(C[i * cols + j] - expected) > 1e-6) {
                std::cerr << "Error at (" << i << "," << j << "): " << C[i * cols + j] << " != " << expected << "\n";
                return false;
            }
        }
    }
    return true;
}

// 初始化矩阵
void init_matrix(float* mat, int rows, int cols) {
    for (int i = 0; i < rows * cols; ++i) {
        mat[i] = static_cast<float>(rand()) / RAND_MAX;
    }
}

int main() {
    const int rows = 4096;
    const int cols = 4096;

    // 分配内存（使用_aligned_malloc保证对齐）
    float* A = static_cast<float*>(std::aligned_alloc(64, rows * cols * sizeof(float)));
    float* B = static_cast<float*>(std::aligned_alloc(64, rows * cols * sizeof(float)));
    float* C1 = static_cast<float*>(std::aligned_alloc(64, rows * cols * sizeof(float)));  // 标量结果
    float* C2 = static_cast<float*>(std::aligned_alloc(64, rows * cols * sizeof(float)));  // AVX2结果

    // 初始化矩阵
    init_matrix(A, rows, cols);
    init_matrix(B, rows, cols);

    // 测试标量版本
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        matrix_add_scalar(A, B, C1, rows, cols);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> scalar_time = end - start;

    // 预热CPU
    for (int i = 0; i < 10; ++i) {
        matrix_add_avx256(A, B, C2, rows, cols);
    }
    // 测试AVX2版本
    auto start_avx2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        matrix_add_avx256(A, B, C2, rows, cols);
    }
    auto end_avx2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> avx2_time = end_avx2 - start_avx2;

    // 验证结果
    if (!verify_result(A, B, C1, rows, cols)) {
        std::cerr << "Scalar version failed verification!\n";
    }
    if (!verify_result(A, B, C2, rows, cols)) {
        std::cerr << "AVX2 version failed verification!\n";
    }

    // 输出耗时比较
    std::cout << "Matrix size: " << rows << "x" << cols << "\n";
    std::cout << "Scalar time: " << scalar_time.count() * 1000 / 100 << " ms\n";
    std::cout << "AVX2 time:   " << avx2_time.count() * 1000 / 100 << " ms\n";
    std::cout << "Speedup:     " << scalar_time.count() / avx2_time.count() << "x\n";

    std::cout << "CPU Cache Sizes (L1/L2/L3): " << sysconf(_SC_LEVEL1_DCACHE_SIZE) / 1024 << "K/"
              << sysconf(_SC_LEVEL2_CACHE_SIZE) / 1024 << "K/" << sysconf(_SC_LEVEL3_CACHE_SIZE) / 1024 << "K\n";

    #ifdef __AVX10_2_512CONVERTINTRIN_H_INCLUDED
    std::cout << "AVX support: __AVX10_2_512CONVERTINTRIN_H_INCLUDED" << std::endl;
    #endif
    #ifdef __AVX512FP16VLINTRIN_H_INCLUDED
    std::cout << "AVX support: __AVX512FP16VLINTRIN_H_INCLUDED" << std::endl;
    #endif

    // 释放内存
    _mm_free(A);
    _mm_free(B);
    _mm_free(C1);
    _mm_free(C2);

    return 0;
}
