#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

struct stress_test_config {
    size_t producer_count{4};
    size_t consumer_count{4};
    size_t items_per_producer{1000};
    uint32_t timeout_seconds{30};
};

inline stress_test_config bounded_fullness_config() {
    return {.producer_count = 8, .consumer_count = 8, .items_per_producer = 50000, .timeout_seconds = 60};
}

inline stress_test_config push_pop_config() {
    return {.producer_count = 2, .consumer_count = 1, .items_per_producer = 10000, .timeout_seconds = 60};
}

inline stress_test_config dequeue_stress_config() {
    return {.producer_count = 0, .consumer_count = 64, .items_per_producer = 10000, .timeout_seconds = 30};
}

inline stress_test_config spsc_config() {
    return {.producer_count = 1, .consumer_count = 1, .items_per_producer = 100000, .timeout_seconds = 30};
}

struct element_type {
    size_t producer_id;
    size_t sequence;
};

class data_validator {
   public:
    explicit data_validator(size_t producer_count, size_t items_per_producer)
        : producer_count_(producer_count),
          items_per_producer_(items_per_producer),
          total_items_(producer_count * items_per_producer),
          bitmap_(new std::atomic<bool>[total_items_]) {
        for (size_t i = 0; i < total_items_; ++i) {
            bitmap_[i].store(false, std::memory_order_relaxed);
        }
    }

    void record_produced() {
        produce_count_.fetch_add(1, std::memory_order_relaxed);
    }

    bool record_consumed(uint32_t producer_id, uint32_t sequence) {
        size_t index = static_cast<size_t>(producer_id) * items_per_producer_ + static_cast<size_t>(sequence);
        if (index >= total_items_) {
            return false;
        }
        bool expected = false;
        if (!bitmap_[index].compare_exchange_strong(expected, true)) {
            return false;
        }
        consume_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    size_t total_produced() const {
        return produce_count_.load(std::memory_order_acquire);
    }

    size_t total_consumed() const {
        return consume_count_.load(std::memory_order_acquire);
    }

    bool validate_no_loss() const {
        return consume_count_.load(std::memory_order_acquire) == total_items_;
    }

   private:
    size_t producer_count_;
    size_t items_per_producer_;
    size_t total_items_;
    std::unique_ptr<std::atomic<bool>[]> bitmap_;
    std::atomic<size_t> produce_count_{0};
    std::atomic<size_t> consume_count_{0};
};

class barrier_sync {
   public:
    explicit barrier_sync(size_t expected) : expected_(expected) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        if (++waiting_count_ >= expected_) {
            released_ = true;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this] { return released_; });
        }
    }

   private:
    std::mutex mtx_;
    std::condition_variable cv_;
    size_t waiting_count_{0};
    size_t expected_;
    bool released_{false};
};

struct sync_context {
    data_validator* validator;
    barrier_sync* barrier;
    std::atomic<size_t>* producers_done;
    size_t total_producers;
};

template <typename Queue>
void produce_items(Queue& queue, size_t producer_id, size_t item_count, sync_context& ctx) {
    ctx.barrier->arrive_and_wait();
    for (size_t i = 0; i < item_count; ++i) {
        element_type value{producer_id, i};
        while (!queue.enqueue(value)) {
            std::this_thread::yield();
        }
        ctx.validator->record_produced();
    }
    ctx.producers_done->fetch_add(1, std::memory_order_release);
}

template <typename Queue>
void validate_consumed(Queue& queue, sync_context& ctx) {
    ctx.barrier->arrive_and_wait();
    element_type value;
    while (true) {
        if (queue.dequeue(value)) {
            ctx.validator->record_consumed(static_cast<uint32_t>(value.producer_id),
                                           static_cast<uint32_t>(value.sequence));
        } else {
            if (ctx.producers_done->load(std::memory_order_acquire) >= ctx.total_producers) {
                break;
            }
            std::this_thread::yield();
        }
    }
}

template <typename Queue>
void count_consumed(Queue& queue, std::atomic<size_t>& count) {
    element_type value;
    while (queue.dequeue(value)) {
        count.fetch_add(1, std::memory_order_relaxed);
    }
}

template <typename Queue>
std::vector<std::future<void>> launch_producers(Queue& queue,
                                                const stress_test_config& config,
                                                sync_context& ctx) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.producer_count);
    for (size_t i = 0; i < config.producer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&queue, i, &config, &ctx]() {
            produce_items(queue, i, config.items_per_producer, ctx);
        }));
    }
    return tasks;
}

template <typename Queue>
std::vector<std::future<void>> launch_validating_consumers(Queue& queue,
                                                           const stress_test_config& config,
                                                           sync_context& ctx) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.consumer_count);
    for (size_t i = 0; i < config.consumer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&queue, &ctx]() {
            validate_consumed(queue, ctx);
        }));
    }
    return tasks;
}

template <typename Queue>
std::vector<std::future<void>> launch_counting_consumers(Queue& queue,
                                                         const stress_test_config& config,
                                                         std::atomic<size_t>& count) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.consumer_count);
    for (size_t i = 0; i < config.consumer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&queue, &count]() {
            count_consumed(queue, count);
        }));
    }
    return tasks;
}

template <typename TaskContainer>
bool wait_for_completion(TaskContainer& tasks, uint32_t timeout_seconds) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    for (auto& task : tasks) {
        if (task.wait_until(deadline) == std::future_status::timeout) {
            return false;
        }
    }
    return true;
}
