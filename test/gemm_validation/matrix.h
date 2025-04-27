#pragma once
#include <cstdlib>

class Matrix {
   public:
    Matrix(size_t r, size_t c);
    ~Matrix();

    size_t rows() const noexcept { return rows_; }
    size_t cols() const noexcept { return cols_; }
    float* data() noexcept { return data_; }
    const float* data() const noexcept { return data_; }
    size_t size() const noexcept { return rows_ * cols_; }
    void fill_random();

    float* operator[](size_t n) { return &data_[n * cols_]; }
    const float* operator[](size_t n) const { return &data_[n * cols_]; }

    bool operator==(const Matrix& other) const;
    Matrix& operator=(const Matrix& other);

   private:
    const std::align_val_t align_{64};
    float* data_{nullptr};
    size_t rows_{0};
    size_t cols_{0};
};

bool verify_matrix_equal(const Matrix& ref, const Matrix& test, float abs_tolerance = 5e-5f,
                         float rel_tolerance = 5e-2f);
