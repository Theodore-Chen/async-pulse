#pragma once

#include <memory>

template <typename T, typename Alloc = std::allocator<int>>
class uninitialized_buffer {
   public:
    using value_type = T;
    using allocator = Alloc;
    using allocator_type = typename std::allocator_traits<allocator>::template rebind_alloc<value_type>;

    template <typename U, typename Alloc2 = allocator>
    struct rebind {
        typedef uninitialized_buffer<U, Alloc> other;
    };

   public:
    uninitialized_buffer(size_t capacity) : capacity_(capacity) {
        if (capacity >= 2 && (capacity & (capacity - 1)) == 0) {
            buffer_ = allocator_type().allocate(capacity);
        } else {
            capacity_ = 0;
            buffer_ = nullptr;
        }
    }
    ~uninitialized_buffer() {
        allocator_type().deallocate(buffer_, capacity_);
    }
    uninitialized_buffer(const uninitialized_buffer&) = delete;
    uninitialized_buffer& operator=(const uninitialized_buffer&) = delete;

    value_type& operator[](size_t i) {
        return buffer_[i];
    }

    const value_type& operator[](size_t i) const {
        return buffer_[i];
    }

    size_t capacity() const noexcept {
        return capacity_;
    }

    value_type* buffer() noexcept {
        return buffer_;
    }

    value_type* buffer() const noexcept {
        return buffer_;
    }

   private:
    value_type* buffer_;
    size_t capacity_;
};
