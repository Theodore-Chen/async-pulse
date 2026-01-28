#pragma once

#include <atomic>
#include <cstddef>

namespace detail {
namespace hp {

struct retired_ptr {
    void* ptr;
    void (*deleter)(void*);

    retired_ptr() noexcept : ptr(nullptr), deleter(nullptr) {}

    retired_ptr(void* p, void (*d)(void*)) noexcept : ptr(p), deleter(d) {}
};

class retired_array {
public:
    retired_array(retired_ptr* arr, size_t capacity) noexcept
        : current_(arr), last_(arr + capacity), retired_(arr) {}

    retired_array(const retired_array&) = delete;
    retired_array& operator=(const retired_array&) = delete;

    size_t capacity() const noexcept { return last_ - retired_; }

    size_t size() const noexcept {
        return current_.load(std::memory_order_relaxed) - retired_;
    }

    bool push(retired_ptr&& p) noexcept {
        retired_ptr* cur = current_.load(std::memory_order_relaxed);
        *cur = p;
        cur++;
        current_.store(cur, std::memory_order_relaxed);
        return cur < last_;
    }

    retired_ptr* first() const noexcept { return retired_; }

    retired_ptr* last() const noexcept {
        return current_.load(std::memory_order_relaxed);
    }

    void reset(size_t size) noexcept {
        current_.store(first() + size, std::memory_order_relaxed);
    }

    bool full() const noexcept {
        return current_.load(std::memory_order_relaxed) == last_;
    }

    static size_t calc_array_size(size_t capacity) {
        return sizeof(retired_ptr) * capacity;
    }

private:
    std::atomic<retired_ptr*> current_;  // 当前写入位置
    retired_ptr* const last_;            // 数组末尾
    retired_ptr* const retired_;         // 数组起始
};

}  // namespace hp
}  // namespace detail
