#include "openblas_gemm.h"
#include <cblas.h>

void openblas_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R) {
    // 临时拷贝 C 到 R（避免修改原矩阵）
    R = C;

    // 直接计算 R = alpha*A*B + beta*R
    cblas_sgemm(
        CblasRowMajor,
        CblasNoTrans,
        CblasNoTrans,
        A.rows(),
        B.cols(),
        A.cols(),
        alpha,
        A.data(),
        A.cols(),
        B.data(),
        B.cols(),
        beta,
        R.data(),  // 结果直接存入 R
        R.cols()
    );
}