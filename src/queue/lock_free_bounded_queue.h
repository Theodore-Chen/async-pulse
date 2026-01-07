#include <atomic>
#include <memory>
#include <optional>
#include <thread>

template <typename T, typename Alloc = std::allocator<int>>
class uninitialized_buffer {
   public:
    using value_type = T;
    using allocator = Alloc;
    using allocator_type = typename std::allocator_traits<allocator>::template rebind_alloc<value_type>;

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
            is_valid_.store(false);
            is_closed_.store(true);
            return;
        }

        for (size_t i = 0; i < capacity; i++) {
            buffer_[i].sequence.store(i, std::memory_order::memory_order_relaxed);
        }
        pos_enqueue_.store(0, std::memory_order::memory_order_relaxed);
        pos_dequeue_.store(0, std::memory_order::memory_order_relaxed);
    }

    ~lock_free_bounded_queue() {
        close();
        clear();
    }

    lock_free_bounded_queue(const lock_free_bounded_queue&) = delete;
    lock_free_bounded_queue(lock_free_bounded_queue&&) = delete;
    lock_free_bounded_queue& operator=(const lock_free_bounded_queue&) = delete;
    lock_free_bounded_queue& operator=(lock_free_bounded_queue&&) = delete;

    bool enqueue(const value_type& val) {
        return enqueue_with([&val](value_type& dest) { new (&dest) value_type(val); });
    }

    bool enqueue(value_type&& val) {
        return enqueue_with([&val](value_type& dest) { new (&dest) value_type(std::move(val)); });
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue_with([&args...](value_type& dest) { new (&dest) value_type(std::forward<Args>(args)...); });
    }

    template <typename Func>
    bool enqueue_with(Func f) {
        return enqueue_impl<true>(f);
    }

    template <typename Func>
    bool try_enqueue_with(Func f) {
        return enqueue_impl<false>(f);
    }

    template <typename Func>
    bool dequeue_with(Func f) {
        return dequeue_impl<true>(f);
    }

    template <typename Func>
    bool try_dequeue_with(Func f) {
        return dequeue_impl<false>(f);
    }

    bool dequeue(value_type& dest) {
        return dequeue_with([&dest](value_type& item) { dest = std::move(item); });
    }

    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        bool success = dequeue_with([&result](value_type& item) { result.emplace(std::move(item)); });
        return success ? result : std::nullopt;
    }

    size_t capacity() {
        return buffer_.capacity();
    }

    size_t size() {
        size_t head = pos_dequeue_.load();
        size_t tail = pos_enqueue_.load();
        return tail > head ? tail - head : 0;
    }

    bool empty() {
        size_t head = pos_dequeue_.load();
        size_t tail = pos_enqueue_.load();
        return head == tail;
    }

    void clear() {
        value_type item;
        while (dequeue(item)) {}
    }

    void close() {
        is_closed_.store(true);
    }

    bool is_closed() {
        return is_closed_.load();
    }

    bool is_valid() {
        return is_valid_.load();
    }

   private:
    template <bool Blocking, typename Func>
    bool enqueue_impl(Func f) {
        if (is_closed_.load() == true) {
            return false;
        }

        size_t pos = pos_enqueue_.load();
        cell_type* cell;

        for (;;) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load();
            int64_t dif = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);
            if (dif == 0) {
                if (pos_enqueue_.compare_exchange_weak(pos, pos + 1)) {
                    break;
                }
            } else if (dif < 0) {
                if constexpr (!Blocking) {
                    if (pos - pos_dequeue_.load() == capacity()) {
                        return false;
                    }
                }
                std::this_thread::yield();
                pos = pos_enqueue_.load();
            } else {
                pos = pos_enqueue_.load();
            }
        }

        f(cell->data);
        cell->sequence.store(pos + 1);

        return true;
    }

    template <bool Blocking, typename Func>
    bool dequeue_impl(Func f) {
        size_t pos = pos_dequeue_.load();
        cell_type* cell;

        for (;;) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load();
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (pos_dequeue_.compare_exchange_weak(pos, pos + 1)) {
                    break;
                }
            } else if (diff < 0) {
                if constexpr (!Blocking) {
                    if (pos - pos_enqueue_.load() == 0) {
                        return false;
                    }
                }
                if (is_closed_.load() == true) {
                    return false;
                }
                std::this_thread::yield();
                pos = pos_dequeue_.load();
            } else {
                pos = pos_dequeue_.load();
            }
        }

        f(cell->data);
        cell->data.~value_type();
        cell->sequence.store(pos + buffer_mask_ + 1);

        return true;
    }

   private:
    uninitialized_buffer<cell_type> buffer_;
    const size_t buffer_mask_;
    alignas(std::hardware_constructive_interference_size) sequence_type pos_enqueue_;
    alignas(std::hardware_constructive_interference_size) sequence_type pos_dequeue_;
    alignas(std::hardware_constructive_interference_size) std::atomic<bool> is_valid_{false};
    std::atomic<bool> is_closed_{false};
};
