#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>

#include "matrix.h"

Matrix::Matrix(size_t r, size_t c) : rows_(r), cols_(c) {
    data_ = static_cast<float*>(operator new[](r * c * sizeof(float), align_, std::nothrow));
}

Matrix::~Matrix() {
    if (data_) {
        operator delete[](data_, align_);
        data_ = nullptr;
    }
}

bool Matrix::operator==(const Matrix& other) const {
    return rows() == other.rows() && cols() == other.cols() && verify_matrix_equal(*this, other);
}

Matrix& Matrix::operator=(const Matrix& other) {
    if (rows_ == other.rows_ && cols_ == other.cols_ && data_ != nullptr && other.data_ != nullptr) {
        std::memcpy(data_, other.data_, other.size());
    } else {
        throw std::runtime_error("Matrix dimensions mismatch in assignment");
    }
    return *this;
}

void Matrix::fill_random() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::generate(data_, data_ + size(), [&]() { return dist(gen); });
}

bool verify_result(const Matrix& ref, const Matrix& test, float tolerance = 1e-6f) {
    for (size_t i = 0; i < ref.rows(); ++i) {
        for (size_t j = 0; j < ref.cols(); ++j) {
            float sub = std::fabs(ref[i][j] - test[i][j]);
            if (sub > tolerance) {
                std::cerr << "Error at (" << i << "," << j << "): " << std::fixed << std::setprecision(6) << ref[i][j]
                          << " vs " << test[i][j] << " (diff: " << sub << ")\n";
                return false;
            }
        }
    }
    return true;
}

bool verify_matrix_equal(const Matrix& ref, const Matrix& test, float abs_tolerance, float rel_tolerance) {
    size_t error_count = 0;
    float max_abs_diff = 0.0f;
    float max_rel_diff = 0.0f;

    for (size_t i = 0; i < ref.rows(); ++i) {
        for (size_t j = 0; j < ref.cols(); ++j) {
            float abs_diff = std::fabs(ref[i][j] - test[i][j]);
            float ref_val = std::fabs(ref[i][j]);
            float test_val = std::fabs(test[i][j]);
            float max_val = std::max(ref_val, test_val);

            // 混合容差：优先绝对容差，若值较大则检查相对容差
            if (abs_diff > abs_tolerance && abs_diff > rel_tolerance * max_val) {
                error_count++;
                if (error_count <= 10) {
                    std::cerr << "Error at (" << i << "," << j << "): " << std::fixed << std::setprecision(6)
                              << ref[i][j] << " vs " << test[i][j] << " (abs_diff: " << abs_diff
                              << ", rel_diff: " << (abs_diff / max_val) << ")\n";
                }
            }

            if (abs_diff > max_abs_diff)
                max_abs_diff = abs_diff;
            if (max_val > 0 && abs_diff / max_val > max_rel_diff) {
                max_rel_diff = abs_diff / max_val;
            }
        }
    }

    if (error_count > 0) {
        std::cerr << "Total errors: " << error_count << "/" << (ref.rows() * ref.cols()) << "\n";
        std::cerr << "Max absolute difference: " << max_abs_diff << "\n";
        std::cerr << "Max relative difference: " << max_rel_diff << "\n";
        return false;
    }
    return true;
}