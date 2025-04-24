#pragma once
#include <algorithm>
#include <random>
#include "matrix.h"

void fill_random(Matrix& mat) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::generate(mat.data(), mat.data() + mat.rows() * mat.cols(), [&]() { return dist(gen); });
}
