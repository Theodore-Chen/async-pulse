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
    lock_free_bounded_queue(size_t capacity) : buffer_(capacity), buffer_mask_(capacity - 1) {
        for (size_t i = 1; i < capacity; i++) {
            buffer_[i].sequence.store(i, std::memory_order::memory_order_relaxed);
        }
        pos_enqueue.store(0, std::memory_order::memory_order_relaxed);
        pos_dequeue.store(0, std::memory_order::memory_order_relaxed);
    }
    ~lock_free_bounded_queue() {}

    lock_free_bounded_queue(const lock_free_bounded_queue&) = delete;
    lock_free_bounded_queue(lock_free_bounded_queue&&) = delete;
    lock_free_bounded_queue& operator=(const lock_free_bounded_queue&) = delete;
    lock_free_bounded_queue& operator=(lock_free_bounded_queue&&) = delete;

    bool enqueue(const value_type& val) {
        for (;;) {
            size_t pos = pos_enqueue.load();
            cell_type* cell = buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load();
            int64_t dif = static_cast<int64_t>(pos) - static_cast<int64_t>(seq);
            if (dif == 0) {
                if (pos_enqueue.compare_exchange_weak(pos, pos + 1)) {
                    break;
                }
            } else if (dif < 0) {
                if (pos - pos_dequeue.load() == capacity())
            }
        }
    }

    size_t capacity() {
        return buffer_.capacity();
    }

   private:
    uninitialized_buffer<value_type> buffer_;
    const size_t buffer_mask_;
    sequence_type pos_enqueue;
    sequence_type pos_dequeue;
};
