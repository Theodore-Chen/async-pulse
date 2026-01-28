#pragma once

#include <cassert>
#include <cstddef>
#include "guard.h"
#include "guard_array.h"

namespace detail {
namespace hp {

class thread_hp_storage {
public:
    thread_hp_storage(guard* arr, size_t size) noexcept
        : free_head_(arr), array_(arr), capacity_(size) {
        for (guard* p = arr; p < arr + size - 1; ++p) {
            p->next_ = p + 1;
        }
        (arr + size - 1)->next_ = nullptr;
    }

    thread_hp_storage(const thread_hp_storage&) = delete;
    thread_hp_storage& operator=(const thread_hp_storage&) = delete;

    size_t capacity() const noexcept { return capacity_; }

    bool full() const noexcept { return free_head_ == nullptr; }

    guard* alloc() {
        assert(!full() && "No available hazard pointer slots");
        guard* g = free_head_;
        free_head_ = g->next_;
        return g;
    }

    void free(guard* g) noexcept {
        if (g) {
            g->clear();
            g->next_ = free_head_;
            free_head_ = g;
        }
    }

    template <size_t Capacity>
    size_t alloc(guard_array<Capacity>& arr) {
        guard* g = free_head_;
        size_t i;
        for (i = 0; i < Capacity && g; ++i) {
            arr.reset(i, g);
            g = g->next_;
        }
        assert(i == Capacity && "Not enough hazard pointer slots");
        free_head_ = g;
        return i;
    }

    template <size_t Capacity>
    void free(guard_array<Capacity>& arr) noexcept {
        guard* gList = free_head_;
        for (size_t i = 0; i < Capacity; ++i) {
            guard* g = arr[i];
            if (g) {
                g->clear();
                g->next_ = gList;
                gList = g;
            }
        }
        free_head_ = gList;
    }

    void clear() {
        for (guard* cur = array_, *last = array_ + capacity_; cur < last; ++cur) {
            cur->clear();
        }
    }

    guard& operator[](size_t idx) {
        assert(idx < capacity());
        return array_[idx];
    }

    guard* begin() const { return array_; }
    guard* end() const { return &array_[capacity_]; }

    static size_t calc_array_size(size_t capacity) {
        return sizeof(guard) * capacity;
    }

private:
    guard* free_head_;
    guard* const array_;
    size_t const capacity_;
};

}  // namespace hp
}  // namespace detail
