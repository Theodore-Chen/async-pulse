#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

struct stress_test_config {
    size_t producer_count{4};
    size_t consumer_count{4};
    size_t items_per_producer{1000};
    uint32_t timeout_seconds{30};
};

inline stress_test_config bounded_fulness_config() {
    return {.producer_count = 8, .consumer_count = 8, .items_per_producer = 50000, .timeout_seconds = 60};
}

inline stress_test_config push_pop_config() {
    return {.producer_count = 2, .consumer_count = 1, .items_per_producer = 10000, .timeout_seconds = 60};
}

inline stress_test_config dequeue_stress_config() {
    return {.producer_count = 0, .consumer_count = 32, .items_per_producer = 10000, .timeout_seconds = 30};
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

    void record_produced(uint32_t, uint32_t) {
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

template <typename Queue>
void producer_worker(Queue& queue,
                     size_t producer_id,
                     size_t item_count,
                     data_validator& validator,
                     barrier_sync& sync,
                     std::atomic<size_t>& producers_done) {
    sync.arrive_and_wait();
    for (size_t i = 0; i < item_count; ++i) {
        element_type value{producer_id, i};
        while (!queue.enqueue(value)) {
            std::this_thread::yield();
        }
        validator.record_produced(static_cast<uint32_t>(producer_id), static_cast<uint32_t>(i));
    }
    producers_done.fetch_add(1, std::memory_order_release);
}

template <typename Queue>
void consumer_worker(
    Queue& queue, data_validator& validator, barrier_sync& sync, std::atomic<size_t>& producers_done, size_t total) {
    sync.arrive_and_wait();
    element_type value;
    while (true) {
        if (queue.dequeue(value)) {
            validator.record_consumed(static_cast<uint32_t>(value.producer_id), static_cast<uint32_t>(value.sequence));
        } else {
            if (producers_done.load(std::memory_order_acquire) >= total) {
                break;
            }
            std::this_thread::yield();
        }
    }
}

template <typename Queue>
void counting_consumer(Queue& queue, std::atomic<size_t>& count) {
    element_type value;
    while (queue.dequeue(value)) {
        count.fetch_add(1, std::memory_order_relaxed);
    }
}

template <typename Queue>
std::vector<std::future<void>> launch_producers(Queue& queue,
                                                const stress_test_config& config,
                                                data_validator& validator,
                                                barrier_sync& sync,
                                                std::atomic<size_t>& producers_done) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.producer_count);
    for (size_t i = 0; i < config.producer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&]() {
            producer_worker(queue, i, config.items_per_producer, validator, sync, producers_done);
        }));
    }
    return tasks;
}

template <typename Queue>
std::vector<std::future<void>> launch_consumers(Queue& queue,
                                                const stress_test_config& config,
                                                data_validator& validator,
                                                barrier_sync& sync,
                                                std::atomic<size_t>& producers_done) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.consumer_count);
    for (size_t i = 0; i < config.consumer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&]() {
            consumer_worker(queue, validator, sync, producers_done, config.producer_count);
        }));
    }
    return tasks;
}

template <typename Queue>
std::vector<std::future<void>> launch_consumers(Queue& queue,
                                                const stress_test_config& config,
                                                std::atomic<size_t>& consumed_count) {
    std::vector<std::future<void>> tasks;
    tasks.reserve(config.consumer_count);
    for (size_t i = 0; i < config.consumer_count; ++i) {
        tasks.emplace_back(std::async(std::launch::async, [&]() { counting_consumer(queue, consumed_count); }));
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

template <typename Queue>
void fill_queue_to_count(Queue& queue, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        queue.enqueue(element_type{0, i});
    }
}
