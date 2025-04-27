#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include "avx2_gemm.h"
#include "cublas_gemm.h"
#include "matrix.h"
#include "openblas_gemm.h"

void reference_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R) {
    for (size_t i = 0; i < A.rows(); ++i) {
        for (size_t j = 0; j < B.cols(); ++j) {
            float sum = 0.0f;
            for (size_t k = 0; k < A.cols(); ++k) {
                sum += A[i][k] * B[k][j];
            }
            R[i][j] = alpha * sum + beta * C[i][j];
        }
    }
}

int main() {
    const size_t M = 2048, K = 2048, N = 2048;
    const float alpha = 1.0f, beta = 1.0f;
    const size_t repeat_time = 100;

    // 初始化矩阵
    Matrix A(M, K), B(K, N), C(M, N), R_ref(M, N), R_avx2(M, N), R_openblas(M, N), R_cublas(M, N);
    A.fill_random();
    B.fill_random();
    C.fill_random();

    using gemm_func = void (*)(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R);
    auto process_gemm = [&A, &B, &C, alpha, beta, repeat_time](gemm_func func, Matrix& R) -> double {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < repeat_time; i++) {
            func(A, B, C, alpha, beta, R);
        }
        auto t2 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(t2 - t1).count() / repeat_time;
    };

    auto print_result = [](Matrix& base_line, double base_time, Matrix& test, double test_time, std::string test_name) {
        bool correct = verify_matrix_equal(base_line, test);
        std::cout << std::left << std::endl;
        std::cout << std::setw(20) << "Verification:" << (correct ? "PASSED" : "FAILED") << "\n";
        std::cout << std::setw(20) << "Matrix size:" << "M = " << M << ", K = " << K << ", N = " << N << "\n";
        std::cout << std::setw(20) << "CPU Time:" << base_time << " ms\n";
        std::cout << std::setw(20) << test_name + " Time:" << test_time << " ms\n";
        std::cout << std::setw(20) << test_name + " Speedup:" << base_time / test_time << "x\n";
    };

    double cpu_time = process_gemm(reference_gemm, R_ref);
    double avx2_time = process_gemm(avx2_gemm, R_avx2);
    print_result(R_ref, cpu_time, R_avx2, avx2_time, "AVX2");

    double openblas_time = process_gemm(openblas_gemm, R_openblas);
    print_result(R_ref, cpu_time, R_openblas, openblas_time, "Openblas");

    double cublas_time = process_gemm(cublas_gemm, R_cublas);
    print_result(R_ref, cpu_time, R_cublas, cublas_time, "Cublas");

    return 0;
}
