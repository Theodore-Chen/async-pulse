#pragma once

#include <cstddef>

#if defined(__cpp_lib_hardware_interference_size)
constexpr size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
constexpr size_t CACHE_LINE_SIZE = 64;
#endif
