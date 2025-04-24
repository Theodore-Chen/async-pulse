#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include "avx2_gemm.h"
#include "matrix.h"
#include "utils.h"

void reference_gemm(const Matrix& A, const Matrix& B, Matrix& C, float alpha, float beta) {
    for (size_t i = 0; i < A.rows(); ++i) {
        for (size_t j = 0; j < B.cols(); ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < A.cols(); ++k) {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = alpha * sum + beta * C[i][j];  // α 和 β 的作用点
        }
    }
}

bool verify_result(const Matrix& ref, const Matrix& test, float tolerance = 1e-6f) {
    for (size_t i = 0; i < ref.rows(); ++i) {
        for (size_t j = 0; j < ref.cols(); ++j) {
            float sub = std::fabs(ref[i][j] - test[i][j]);
            if (sub > tolerance) {
                std::cerr << "Error at (" << i << "," << j << "): "
                          << std::fixed << std::setprecision(6)
                          << ref[i][j] << " vs " << test[i][j] << " (diff: " << sub << ")\n";
                return false;
            }
        }
    }
    return true;
}

int main() {
    const size_t M = 512, K = 512, N = 512;
    const float alpha = 1.0f, beta = 1.0f;  // 重点测试 β=1 的情况
    const size_t repeat_time = 10;

    // 初始化矩阵
    Matrix A(M, K), B(K, N), C_ref(M, N), C_cublas(M, N), C_avx2(M, N);
    fill_random(A);
    fill_random(B);
    fill_random(C_ref);
    C_cublas = C_avx2 = C_ref;

    using gemm_func = void (*)(const Matrix& A, const Matrix& B, Matrix& C, float alpha, float beta);
    auto process_gemm = [&A, &B, alpha, beta](Matrix& C, gemm_func func) -> double {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < repeat_time; i++) {
            func(A, B, C, alpha, beta);
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t2 - t1).count() / repeat_time;
    };

    // 计算参考结果（CPU）
    double cpu_time = process_gemm(C_ref, reference_gemm);

    // 计算 avx2 结果（CPU）
    double avx2_time = process_gemm(C_avx2, avx2_gemm);

    // 计算 cuBLAS 结果
    // t1 = std::chrono::high_resolution_clock::now();
    // cublas_gemm(A, B, C_cublas, alpha, beta);
    // t2 = std::chrono::high_resolution_clock::now();
    // double cuda_time = std::chrono::duration<double, std::milli>(t2 - t1).count();

    // 验证结果
    bool is_correct = verify_result(C_ref, C_avx2);
    std::cout << "Verification: " << (is_correct ? "PASSED" : "FAILED") << "\n";
    std::cout << "Matrix size:  " << "M = "<< M << ", K = " << K << ", N = " << N << "\n";
    std::cout << "CPU Time:     " << cpu_time << " ms\n";
    std::cout << "AVX2 Time:    " << avx2_time << " ms\n";
    std::cout << "AVX2 Speedup: " << cpu_time / avx2_time << "x\n";

    // std::cout << "cuBLAS Time: " << cuda_time << " ms\n";

    return is_correct ? 0 : 1;
}
