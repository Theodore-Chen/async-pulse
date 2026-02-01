#pragma once

#include <array>

using small_object = int;

struct medium_object {
    std::array<int, 16> data{};
    int value{0};

    medium_object() = default;
    explicit medium_object(int v) : value(v) {}
};

struct large_object {
    std::array<int, 1024> data{};

    large_object() = default;
    explicit large_object(int v) {
        data[0] = v;
    }
};
