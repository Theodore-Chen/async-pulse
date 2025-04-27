#pragma once
#include "matrix.h"

void openblas_gemm(const Matrix& A, const Matrix& B, const Matrix& C, float alpha, float beta, Matrix& R);