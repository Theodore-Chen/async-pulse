#pragma once

#include <cassert>
#include <cstddef>
#include "guard.h"

namespace detail {
namespace hp {

template <size_t Capacity>
class guard_array {
public:
    static constexpr size_t c_nCapacity = Capacity;

    guard_array() : arr_{} {}

    static constexpr size_t capacity() { return c_nCapacity; }

    guard* operator[](size_t idx) const noexcept {
        assert(idx < capacity());
        return arr_[idx];
    }

    template <typename T>
    void set(size_t idx, T* ptr) noexcept {
        assert(idx < capacity());
        assert(arr_[idx] != nullptr);
        arr_[idx]->set(ptr);
    }

    void clear(size_t idx) noexcept {
        assert(idx < capacity());
        assert(arr_[idx] != nullptr);
        arr_[idx]->clear();
    }

    guard* release(size_t idx) noexcept {
        assert(idx < capacity());
        guard* g = arr_[idx];
        arr_[idx] = nullptr;
        return g;
    }

    void reset(size_t idx, guard* g) noexcept {
        assert(idx < capacity());
        assert(arr_[idx] == nullptr);
        arr_[idx] = g;
    }

private:
    guard* arr_[c_nCapacity];
};

}  // namespace hp
}  // namespace detail
