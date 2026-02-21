#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>
#include <optional>
#include <utility>

#include "opt/back_off.h"
#include "opt/buffer.h"
#include "opt/cache_line.h"

/**
 * @brief FastFlow-style lock-free bounded MPMC queue
 *
 * This implementation is based on FastFlow's MPMC_Ptr_Queue, which originated
 * from Dmitry Vyukov's bounded MPMC queue design (www.1024cores.net).
 *
 * Key design features:
 * - Each slot occupies its own cache line to eliminate false sharing between slots
 * - Enqueue and dequeue positions are cache-line aligned using union trick
 * - Uses sequence numbers for lock-free synchronization
 * - Capacity must be a power of 2
 *
 * @tparam T Element type
 * @tparam Buffer Buffer type for storage
 * @tparam BackOff Back-off strategy for contention
 */
template <typename T, typename Buffer = uninitialized_buffer<void*>, typename BackOff = back_off<>>
class ff_bounded_queue {
   public:
    using value_type = T;
    using sequence_type = std::atomic<size_t>;

    /**
     * @brief Cache-line aligned cell structure
     *
     * Each cell occupies exactly one cache line to prevent false sharing
     * between adjacent slots when accessed by different threads.
     */
    struct alignas(CACHE_LINE_SIZE) cell_type {
        sequence_type sequence;
        value_type data;

        cell_type() {}
    };

    using buffer_type = typename Buffer::template rebind<cell_type>::other;
    using back_off_strategy = BackOff;

   public:
    /**
     * @brief Construct a ff_bounded_queue with the given capacity
     *
     * @param capacity Queue capacity (must be power of 2 and >= 2)
     *
     * Initializes sequence numbers: cell[i].sequence = i
     * This means cell is ready for enqueue at position i
     */
    explicit ff_bounded_queue(size_t capacity)
        : buffer_(capacity), buffer_mask_(capacity - 1), capacity_(capacity), is_closed_(false) {
        assert(capacity >= 2 && (capacity & (capacity - 1)) == 0 && "capacity must be power of 2 and >= 2");

        // Initialize sequence for each cell: cell[i].sequence = i
        // This means the cell is ready for enqueue at position i
        for (size_t i = 0; i < capacity; i++) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Destructor - closes the queue and drains remaining elements
     */
    ~ff_bounded_queue() {
        close();
        // Drain remaining elements
        while (try_dequeue_with([](value_type&) {})) {}
    }

    // Disable copy operations
    ff_bounded_queue(const ff_bounded_queue&) = delete;
    ff_bounded_queue(ff_bounded_queue&&) = delete;
    ff_bounded_queue& operator=(const ff_bounded_queue&) = delete;
    ff_bounded_queue& operator=(ff_bounded_queue&&) = delete;

    // ==================== State Queries ====================

    /**
     * @brief Get the queue capacity
     * @return Maximum number of elements the queue can hold
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief Get the current number of elements in the queue
     * @return Approximate number of elements (may be imprecise in concurrent scenarios)
     */
    size_t size() const {
        size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);
        size_t deq_pos = dequeue_pos_.load(std::memory_order_acquire);
        int64_t diff = static_cast<int64_t>(enq_pos) - static_cast<int64_t>(deq_pos);
        return diff > 0 ? static_cast<size_t>(diff) : 0;
    }

    /**
     * @brief Check if the queue is empty
     * @return true if queue is empty, false otherwise
     */
    bool empty() const {
        size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);
        size_t deq_pos = dequeue_pos_.load(std::memory_order_acquire);
        // In concurrent scenarios, deq_pos can temporarily exceed enq_pos
        // when consumers have claimed slots but producers haven't yet
        return enq_pos <= deq_pos;
    }

    /**
     * @brief Check if the queue is full
     * @return true if queue is full, false otherwise
     */
    bool is_full() const {
        return size() >= capacity_;
    }

    // ==================== Lifecycle ====================

    /**
     * @brief Close the queue
     *
     * Once closed, no more elements can be enqueued.
     * Dequeue operations can still drain remaining elements.
     */
    void close() {
        is_closed_.store(true, std::memory_order_release);
    }

    /**
     * @brief Check if the queue is closed
     * @return true if queue is closed, false otherwise
     */
    bool is_closed() const {
        return is_closed_.load(std::memory_order_acquire);
    }

    // ==================== Enqueue Operations ====================

    /**
     * @brief Enqueue an element by copying (left-value)
     * @param val The value to enqueue
     * @return true if successful, false if queue is closed
     */
    bool enqueue(const value_type& val) {
        return enqueue_with([&val](value_type& dest) { new (&dest) value_type(val); });
    }

    /**
     * @brief Enqueue an element by moving (right-value)
     * @param val The value to enqueue
     * @return true if successful, false if queue is closed
     */
    bool enqueue(value_type&& val) {
        return enqueue_with([&val](value_type& dest) { new (&dest) value_type(std::move(val)); });
    }

    /**
     * @brief Construct an element in-place
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return true if successful, false if queue is closed
     */
    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue_with([&args...](value_type& dest) { new (&dest) value_type(std::forward<Args>(args)...); });
    }

    /**
     * @brief Enqueue with a callback function (blocking)
     *
     * The callback is called with a reference to the cell's data storage
     * where the value should be constructed.
     *
     * @tparam Func Callback function type
     * @param f The callback that constructs the value in-place
     * @return true if successful, false if queue is closed
     */
    template <typename Func>
    bool enqueue_with(Func f) {
        return enqueue_impl<true>(f);
    }

    /**
     * @brief Try to enqueue with a callback function (non-blocking)
     *
     * If the queue is full, returns immediately without blocking.
     *
     * @tparam Func Callback function type
     * @param f The callback that constructs the value in-place
     * @return true if successful, false if queue is full or closed
     */
    template <typename Func>
    bool try_enqueue_with(Func f) {
        return enqueue_impl<false>(f);
    }

    // ==================== Dequeue Operations ====================

    /**
     * @brief Dequeue an element by reference (blocking)
     *
     * @param dest Reference to store the dequeued element
     * @return true if element was dequeued, false if queue is closed and empty
     */
    bool dequeue(value_type& dest) {
        return dequeue_with([&dest](value_type& item) { dest = std::move(item); });
    }

    /**
     * @brief Dequeue an element returning optional (blocking)
     *
     * @return std::optional containing the element, or nullopt if queue is closed and empty
     */
    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        bool success = dequeue_with([&result](value_type& item) { result.emplace(std::move(item)); });
        return success ? result : std::nullopt;
    }

    /**
     * @brief Dequeue with callback (blocking)
     *
     * @tparam Func Callback function type
     * @param f Callback function that receives the dequeued element
     * @return true if element was dequeued, false if queue is closed and empty
     */
    template <typename Func>
    bool dequeue_with(Func f) {
        return dequeue_impl<true>(f);
    }

    /**
     * @brief Try dequeue with callback (non-blocking)
     *
     * @tparam Func Callback function type
     * @param f Callback function that receives the dequeued element
     * @return true if element was dequeued, false if queue is empty
     */
    template <typename Func>
    bool try_dequeue_with(Func f) {
        return dequeue_impl<false>(f);
    }

   private:
    /**
     * @brief Enqueue implementation template
     *
     * FastFlow-style enqueue algorithm:
     * 1. Load current enqueue position
     * 2. Get cell and load its sequence
     * 3. Check if sequence == pos (slot is ready for writing)
     * 4. Try to CAS enqueue_pos from pos to pos + 1
     * 5. If successful, write data and update sequence to pos + 1
     *
     * For blocking mode: spins until slot is ready or queue is closed
     * For non-blocking mode: returns false immediately if queue is full
     *
     * @tparam Blocking true for blocking, false for non-blocking
     * @tparam Func Callback function type
     * @param f Callback function to construct the element in-place
     * @return true if element was enqueued, false otherwise
     */
    template <bool Blocking, typename Func>
    bool enqueue_impl(Func f) {
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        back_off_strategy bkoff;
        size_t pos = enqueue_pos_.load(std::memory_order_acquire);
        cell_type* cell;

        for (;;) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);

            if (diff == 0) {
                // Slot is ready for writing, try to claim it
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is full (slot not yet consumed by consumer)
                if constexpr (!Blocking) {
                    // Non-blocking: check if queue is really full
                    if (pos - dequeue_pos_.load(std::memory_order_acquire) >= capacity_) {
                        return false;
                    }
                }
                bkoff();
                pos = enqueue_pos_.load(std::memory_order_acquire);
            } else {
                // diff > 0: we're behind, reload position
                pos = enqueue_pos_.load(std::memory_order_acquire);
            }
        }

        // Construct the data using callback
        f(cell->data);

        // Signal that data is ready: sequence = pos + 1
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Dequeue implementation template
     *
     * FastFlow-style dequeue algorithm:
     * 1. Load current dequeue position
     * 2. Get cell and load its sequence
     * 3. Check if sequence == pos + 1 (data is ready)
     * 4. Try to CAS dequeue_pos from pos to pos + 1
     * 5. If successful, read data and update sequence to pos + capacity
     *
     * For blocking mode: spins until data is ready or queue is closed
     * For non-blocking mode: returns false immediately if queue is empty
     *
     * @tparam Blocking true for blocking, false for non-blocking
     * @tparam Func Callback function type
     * @param f Callback function to process the dequeued element
     * @return true if element was dequeued, false otherwise
     */
    template <bool Blocking, typename Func>
    bool dequeue_impl(Func f) {
        back_off_strategy bkoff;
        size_t pos = dequeue_pos_.load(std::memory_order_acquire);
        cell_type* cell;

        for (;;) {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                // Data is ready, try to claim this slot
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is empty (data not ready yet)
                if constexpr (!Blocking) {
                    // Non-blocking: check if there are any producers ahead
                    size_t enqueue_pos = enqueue_pos_.load(std::memory_order_acquire);
                    if (pos == enqueue_pos) {
                        return false;
                    }
                }
                // Blocking or queue might have data: check if closed
                if (is_closed_.load(std::memory_order_acquire)) {
                    return false;
                }
                bkoff();
                pos = dequeue_pos_.load(std::memory_order_acquire);
            } else {
                // diff > 0: we're behind, reload position
                pos = dequeue_pos_.load(std::memory_order_acquire);
            }
        }

        // Read the data using callback
        f(cell->data);

        // Destroy the data
        cell->data.~value_type();

        // Signal that slot is ready for reuse: sequence = pos + capacity
        cell->sequence.store(pos + capacity_, std::memory_order_release);

        return true;
    }

    buffer_type buffer_;
    const size_t buffer_mask_;
    const size_t capacity_;

    // Use union trick to ensure each position pointer occupies a full cache line
    // This prevents false sharing between enqueue_pos_ and dequeue_pos_
    union {
        std::atomic<size_t> enqueue_pos_;
        char padding1_[CACHE_LINE_SIZE];
    };

    union {
        std::atomic<size_t> dequeue_pos_;
        char padding2_[CACHE_LINE_SIZE];
    };

    alignas(CACHE_LINE_SIZE) std::atomic<bool> is_closed_;
};
