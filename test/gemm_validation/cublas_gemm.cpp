#include <cuda_runtime.h>
#include <cublas_v2.h>
#include "cublas_gemm.h"

void cublas_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R) {
    // 初始化 cuBLAS
    cublasHandle_t handle;
    cublasCreate(&handle);

    // 分配设备内存
    float *d_A, *d_B, *d_C, *d_R;
    cudaMalloc((void**)&d_A, A.rows() * A.cols() * sizeof(float));
    cudaMalloc((void**)&d_B, B.rows() * B.cols() * sizeof(float));
    cudaMalloc((void**)&d_C, C.rows() * C.cols() * sizeof(float));
    cudaMalloc((void**)&d_R, R.rows() * R.cols() * sizeof(float));

    // 拷贝数据到设备
    cublasSetMatrix(A.rows(), A.cols(), sizeof(float), A.data(), A.cols(), d_A, A.cols());
    cublasSetMatrix(B.rows(), B.cols(), sizeof(float), B.data(), B.cols(), d_B, B.cols());
    cublasSetMatrix(C.rows(), C.cols(), sizeof(float), C.data(), C.cols(), d_C, C.cols());

    // 调用 cuBLAS 的 sgemm
    cublasSgemm(
        handle,
        CUBLAS_OP_N,  // A 不转置
        CUBLAS_OP_N,  // B 不转置
        A.rows(),     // 结果矩阵的行数
        B.cols(),     // 结果矩阵的列数
        A.cols(),     // A 的列数/B 的行数
        &alpha,
        d_A, A.cols(),
        d_B, B.cols(),
        &beta,
        d_C, C.cols()  // 结果直接存入 d_C（对应 R）
    );

    // 拷贝结果回主机
    cublasGetMatrix(R.rows(), R.cols(), sizeof(float), d_C, R.cols(), R.data(), R.cols());

    // 释放设备内存
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
    cudaFree(d_R);

    // 销毁 cuBLAS 句柄
    cublasDestroy(handle);
}