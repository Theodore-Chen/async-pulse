#include "matrix.h"
#include <cublas_v2.h>

void cublas_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R);