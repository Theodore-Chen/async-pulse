#pragma once

#include <atomic>
#include <cassert>
#include <optional>

#include "opt/back_off.h"
#include "opt/buffer.h"
#include "opt/cache_line.h"

/**
 * @brief FAA-based lock-free bounded queue
 *
 * This implementation uses Fetch-And-Add (FAA) operations instead of
 * Compare-And-Swap (CAS) for slot acquisition, which provides:
 * - Better performance under contention (no CAS retry loops)
 * - Fair slot allocation (strict FIFO ordering)
 * - Better cache behavior (producers/consumers increment their own counters)
 *
 * Algorithm overview:
 * - Blocking enqueue: fetch_add(enqueue_pos_) to get slot, wait for slot ready, write data
 * - Blocking dequeue: fetch_add(dequeue_pos_) to get slot, wait for data ready, read data
 * - Non-blocking: check condition first, then CAS if likely to succeed
 *
 * Based on Dmitry Vyukov's bounded MPMC queue with FAA optimization.
 */
template <typename T, typename Buffer = uninitialized_buffer<void*>, typename BackOff = back_off<>>
class faa_bounded_queue {
   public:
    using value_type = T;
    using sequence_type = typename std::atomic<size_t>;
    struct cell_type {
        sequence_type sequence;
        value_type data;

        cell_type() {}
    };
    using buffer_type = typename Buffer::template rebind<cell_type>::other;
    using back_off_strategy = BackOff;

   public:
    explicit faa_bounded_queue(size_t capacity)
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

    ~faa_bounded_queue() {
        close();
        // Drain remaining elements
        while (try_dequeue_with([](value_type&) {})) {}
    }

    faa_bounded_queue(const faa_bounded_queue&) = delete;
    faa_bounded_queue(faa_bounded_queue&&) = delete;
    faa_bounded_queue& operator=(const faa_bounded_queue&) = delete;
    faa_bounded_queue& operator=(faa_bounded_queue&&) = delete;

    // ==================== Enqueue Operations ====================

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

    // ==================== Dequeue Operations ====================

    bool dequeue(value_type& dest) {
        return dequeue_with([&dest](value_type& item) { dest = std::move(item); });
    }

    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        bool success = dequeue_with([&result](value_type& item) { result.emplace(std::move(item)); });
        return success ? result : std::nullopt;
    }

    template <typename Func>
    bool dequeue_with(Func f) {
        return dequeue_impl<true>(f);
    }

    template <typename Func>
    bool try_dequeue_with(Func f) {
        return dequeue_impl<false>(f);
    }

    // ==================== State Queries ====================

    size_t capacity() const {
        return capacity_;
    }

    size_t size() const {
        size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);
        size_t deq_pos = dequeue_pos_.load(std::memory_order_acquire);
        int64_t diff = static_cast<int64_t>(enq_pos) - static_cast<int64_t>(deq_pos);
        return diff > 0 ? static_cast<size_t>(diff) : 0;
    }

    bool empty() const {
        size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);
        size_t deq_pos = dequeue_pos_.load(std::memory_order_acquire);
        // In concurrent FAA scenario, deq_pos can temporarily exceed enq_pos
        // when consumers have claimed slots but producers haven't yet
        return enq_pos <= deq_pos;
    }

    bool is_full() const {
        return size() >= capacity_;
    }

    // ==================== Lifecycle ====================

    void close() {
        is_closed_.store(true, std::memory_order_release);
    }

    bool is_closed() const {
        return is_closed_.load(std::memory_order_acquire);
    }

   private:
    // ==================== Implementation Details ====================

    /**
     * @brief FAA-based enqueue implementation
     */
    template <bool Blocking, typename Func>
    bool enqueue_impl(Func f) {
        if (is_closed_.load(std::memory_order_acquire)) {
            return false;
        }

        size_t pos;
        cell_type* cell;
        back_off_strategy bkoff;

        if constexpr (Blocking) {
            // Blocking: FAA first, then wait
            pos = enqueue_pos_.fetch_add(1, std::memory_order_acq_rel);
            cell = &buffer_[pos & buffer_mask_];

            // Wait for slot to be ready
            for (;;) {
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos);

                if (diff == 0) {
                    break; // Slot is ready
                }

                if (diff < 0) {
                    // Slot not yet consumed, queue is full for this slot
                    if (is_closed_.load(std::memory_order_acquire)) {
                        // Queue closed while waiting, but we should still complete
                        // our write if possible to not lose data
                    }
                    bkoff();
                } else {
                    bkoff();
                }
            }
        } else {
            // Non-blocking: check first, then CAS
            for (;;) {
                pos = enqueue_pos_.load(std::memory_order_acquire);
                size_t deq_pos = dequeue_pos_.load(std::memory_order_acquire);

                // Check if queue is full
                if (pos - deq_pos >= capacity_) {
                    return false;
                }

                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel)) {
                    break;
                }
            }
            cell = &buffer_[pos & buffer_mask_];

            // Wait for slot to be ready
            for (;;) {
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                if (static_cast<int64_t>(seq) == static_cast<int64_t>(pos)) {
                    break;
                }
                bkoff();
            }
        }

        // Construct the data
        f(cell->data);

        // Signal that data is ready
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief FAA-based dequeue implementation
     *
     * Key insight for closing semantics:
     * - When closed, if we've already claimed a slot (via FAA), we should wait for
     *   data if there's a producer that will write to it (pos < enqueue_pos).
     * - If no producer will write to our slot (pos >= enqueue_pos), return false.
     */
    template <bool Blocking, typename Func>
    bool dequeue_impl(Func f) {
        size_t pos;
        cell_type* cell;
        back_off_strategy bkoff;

        if constexpr (Blocking) {
            // Blocking: FAA first, then wait
            pos = dequeue_pos_.fetch_add(1, std::memory_order_acq_rel);
            cell = &buffer_[pos & buffer_mask_];

            // Wait for data to be ready
            for (;;) {
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(pos + 1);

                if (diff == 0) {
                    break; // Data is ready
                }

                if (diff < 0) {
                    // Data not ready yet
                    if (is_closed_.load(std::memory_order_acquire)) {
                        // Queue is closed. Check if any producer will write to our slot.
                        size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);
                        if (pos >= enq_pos) {
                            // No producer will write to our slot, return false
                            return false;
                        }
                        // A producer has claimed a slot <= ours, wait for data
                    }
                    bkoff();
                } else {
                    bkoff();
                }
            }
        } else {
            // Non-blocking: check first, then CAS
            for (;;) {
                pos = dequeue_pos_.load(std::memory_order_acquire);
                size_t enq_pos = enqueue_pos_.load(std::memory_order_acquire);

                // Check if queue is empty
                if (pos >= enq_pos) {
                    return false;
                }

                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel)) {
                    break;
                }
            }
            cell = &buffer_[pos & buffer_mask_];

            // Wait for data to be ready
            for (;;) {
                size_t seq = cell->sequence.load(std::memory_order_acquire);
                if (static_cast<int64_t>(seq) == static_cast<int64_t>(pos + 1)) {
                    break;
                }
                bkoff();
            }
        }

        // Read the data
        f(cell->data);

        // Destroy the data
        cell->data.~value_type();

        // Signal that slot is ready for reuse
        cell->sequence.store(pos + capacity_, std::memory_order_release);

        return true;
    }

   private:
    buffer_type buffer_;
    const size_t buffer_mask_;
    const size_t capacity_;

    // Cache line aligned to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> enqueue_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> dequeue_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<bool> is_closed_;
};
