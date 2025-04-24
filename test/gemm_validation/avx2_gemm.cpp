#include "avx2_gemm.h"
#include <immintrin.h>
#include <algorithm>
#include <cmath>

// AVX2 优化的 GEMM 实现 (α*A*B + β*C)
void avx2_gemm(const Matrix& A, const Matrix& B, Matrix& C, float alpha, float beta) {
    // 检查矩阵维度是否匹配
    if (A.cols() != B.rows() || A.rows() != C.rows() || B.cols() != C.cols()) {
        throw std::runtime_error("Matrix dimensions mismatch in GEMM");
    }

    const size_t M = A.rows();
    const size_t N = B.cols();
    const size_t K = A.cols();

    // 首先应用β*C
    if (beta != 1.0f) {
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < N; ++j) {
                C[i][j] *= beta;
            }
        }
    }

    // AVX2 GEMM核心计算
    constexpr size_t AVX2_FLOAT_NUM = 8;  // AVX2寄存器可以容纳8个float
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < N; j += AVX2_FLOAT_NUM) {
            __m256 c = _mm256_setzero_ps();

            // 计算A的第i行与B的第j列的8个元素的点积
            for (size_t k = 0; k < K; ++k) {
                __m256 a = _mm256_set1_ps(A[i][k] * alpha);
                __m256 b = _mm256_load_ps(&B[k][j]);
                c = _mm256_fmadd_ps(a, b, c);
            }

            // 将结果与β*C相加并存储
            __m256 c_old = _mm256_load_ps(&C[i][j]);
            c = _mm256_add_ps(c, c_old);
            _mm256_store_ps(&C[i][j], c);
        }
    }

    // 处理剩余不足8个元素的情况
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = N - (N % AVX2_FLOAT_NUM); j < N; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; ++k) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = alpha * sum + beta * C[i][j];
        }
    }
}