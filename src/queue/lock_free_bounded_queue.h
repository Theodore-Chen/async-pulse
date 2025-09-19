#include <atomic>
#include <memory>

template <typename T, typename Alloc = std::allocator<int>>
class uninitialized_buffer {
   public:
    using value_type = T;
    using allocator = Alloc;
    using allocator_type = typename std::allocator_traits<allocator>::template rebind_alloc<value_type>;

   public:
    uninitialized_buffer(size_t capacity) : capacity_(capacity) {
        buffer_ = allocator_type().allocator(capacity);
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
    const size_t capacity_;
};

template <typename T>
class lock_free_bounded_queue {
   public:
    using value_type = T;
    using sequence_type = typename std::atomic<size_t>;
    struct cell_type {
        sequence_type sequence;
        value_type data;

        cell_type() {}
    };

   public:
    lock_free_bounded_queue(size_t capacity) {}

   private:
    uninitialized_buffer<value_type>* buffer_;
};
