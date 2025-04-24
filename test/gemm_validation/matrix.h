#pragma once
#include <cstdlib>
#include <cstring>
#include <stdexcept>

class Matrix {
   public:
    Matrix(size_t r, size_t c) : rows_(r), cols_(c) {
        data_ = static_cast<float*>(operator new[](r * c * sizeof(float), std::align_val_t(64), std::nothrow));
    }

    ~Matrix() {
        if (data_) {
            operator delete[](data_, std::align_val_t(64));
            data_ = nullptr;
        }
    }

    size_t rows() const noexcept { return rows_; }
    size_t cols() const noexcept { return cols_; }
    float* data() noexcept { return data_; }
    const float* data() const noexcept { return data_; }
    size_t size() const noexcept { return rows_ * cols_; }

    float* operator[](size_t n) { return &data_[n * cols_]; }
    const float* operator[](size_t n) const { return &data_[n * cols_]; }

    bool operator==(const Matrix& other) const {
        return rows() == other.rows() && cols() == other.cols() &&
               std::memcmp(static_cast<void*>(data_), static_cast<void*>(other.data_), size() * sizeof(float)) == 0;
    }

    Matrix& operator=(const Matrix& other) {
        if (rows_ == other.rows_ && cols_ == other.cols_) {
            std::memcpy(data_, other.data_, other.size());
        } else {
            throw std::runtime_error("Matrix dimensions mismatch in assignment");
        }
        return *this;
    }

   private:
    float* data_{nullptr};
    size_t rows_{0};
    size_t cols_{0};
};
