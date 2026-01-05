#include <atomic>
#include <memory>
#include <thread>

template <typename T, typename Alloc = std::allocator<int>>
class uninitialized_buffer {
   public:
    using value_type = T;
    using allocator = Alloc;
    using allocator_type = typename std::allocator_traits<allocator>::template rebind_alloc<value_type>;

   public:
    uninitialized_buffer(size_t capacity) : capacity_(capacity) {
        buffer_ = allocator_type().allocate(capacity);
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
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            is_valid_ = false;
            return;
        }

        for (size_t i = 0; i < capacity; i++) {
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
        return enqueue_with([&val](value_type& dest) { new (dest) value_type(val); });
    }

    bool enqueue(value_type&& val) {
        return enqueue_with([&val](value_type& dest) { new (dest) value_type(std::move(val)); });
    }

    template <typename Func>
    bool enqueue_with(Func f) {
        size_t pos = pos_enqueue.load();
        cell_type* cell;

        for (;;) {
            cell = buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load();
            int64_t dif = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);
            if (dif == 0) {
                if (pos_enqueue.compare_exchange_weak(pos, pos + 1)) {
                    break;
                }
            } else if (dif < 0) {
                if (pos - pos_dequeue.load() == capacity()) {
                    return false;
                }
                std::this_thread::yield();
                pos = pos_enqueue.load();
            } else {
                pos = pos_enqueue.load();
            }
        }

        f(cell->data);
        cell->sequence.store(pos + 1);
        ++item_count_;

        return true;
    }

    template <typename Func>
    bool dequeue_with(Func f) {
        size_t pos = pos_dequeue.load();
        cell_type* cell;

        for (;;) {
            cell = buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load();
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (pos_dequeue.compare_exchange_weak(pos, pos + 1)) {
                    break;
                }
            } else if (diff < 0) {
                if (pos - pos_enqueue.load() == 0) {
                    return false;
                }
                std::this_thread::yield();
                pos = pos_dequeue.load();
            } else {
                pos = pos_dequeue.load();
            }
        }

        f(cell->data);
        cell->sequence.store(pos + buffer_mask_ + 1);
        --item_count_;

        return true;
    }

    size_t capacity() {
        return buffer_.capacity();
    }

   private:
    uninitialized_buffer<value_type> buffer_;
    const size_t buffer_mask_;
    std::atomic<size_t> item_count_{0};
    sequence_type pos_enqueue;
    sequence_type pos_dequeue;
    bool is_valid_{false};
    bool is_closed_{false};
};
