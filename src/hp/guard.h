#pragma once

#include <atomic>

namespace detail {
namespace hp {

class guard {
public:
    guard() noexcept : hp_(nullptr), next_(nullptr) {}

    void* get() const noexcept { return hp_.load(std::memory_order_acquire); }
    void* get(std::memory_order order) const noexcept { return hp_.load(order); }

    template <typename T>
    T* get_as() const noexcept { return reinterpret_cast<T*>(get()); }

    template <typename T>
    void set(T* ptr) noexcept { hp_.store(reinterpret_cast<void*>(ptr), std::memory_order_release); }

    void clear(std::memory_order order) noexcept { hp_.store(nullptr, order); }
    void clear() noexcept { clear(std::memory_order_release); }

private:
    std::atomic<void*> hp_;

public:
    guard* next_;
};

}  // namespace hp
}  // namespace detail
