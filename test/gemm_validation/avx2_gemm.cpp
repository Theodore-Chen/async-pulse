#include <immintrin.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "avx2_gemm.h"

// AVX2 优化的 GEMM 实现 (α*A*B + β*C)
void avx2_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R) {
    // 检查矩阵维度是否匹配
    if (A.cols() != B.rows() || A.rows() != C.rows() || B.cols() != C.cols()) {
        throw std::runtime_error("Matrix dimensions mismatch in GEMM");
    }

    const size_t M = A.rows();
    const size_t N = B.cols();
    const size_t K = A.cols();

    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < N; j += 8) {    // AVX2一次处理8个float
            __m256 sum = _mm256_setzero_ps();  // 初始化累加器为0

            // 核心计算：sum = A[i][k] * B[k][j:j+8]
            for (size_t k = 0; k < K; ++k) {
                __m256 a = _mm256_broadcast_ss(&A[i][k]);  // 广播A[i][k]到8个位置
                __m256 b = _mm256_load_ps(&B[k][j]);       // 加载B的8个连续元素
                sum = _mm256_fmadd_ps(a, b, sum);          // 乘加运算：sum += a * b
            }

            // 结果融合：R = alpha * sum + beta * C
            __m256 c = _mm256_load_ps(&C[i][j]);
            __m256 res = _mm256_fmadd_ps(_mm256_set1_ps(alpha), sum, _mm256_mul_ps(_mm256_set1_ps(beta), c));
            _mm256_store_ps(&R[i][j], res);  // 存储结果
        }

        // 处理尾部不足8列的情况
        for (size_t j = N - (N % 8); j < N; ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; ++k) {
                sum += A[i][k] * B[k][j];
            }
            R[i][j] = alpha * sum + beta * C[i][j];
        }
    }
}
