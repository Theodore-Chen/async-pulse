#include "matrix.h"

void avx2_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R);