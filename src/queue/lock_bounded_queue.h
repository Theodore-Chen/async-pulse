#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>

enum class queue_status : uint32_t { SUCCESS, FULL, CLOSED };

template <typename T>
class lock_bounded_queue {
   public:
    using value_type = T;

   public:
    lock_bounded_queue(size_t capacity = 0) : capacity_(capacity) {}
    ~lock_bounded_queue() {
        close();
        clear();
    }

    lock_bounded_queue(const lock_bounded_queue&) = delete;
    lock_bounded_queue(lock_bounded_queue&&) = delete;
    lock_bounded_queue& operator=(const lock_bounded_queue&) = delete;
    lock_bounded_queue& operator=(lock_bounded_queue&&) = delete;

    template <typename U>
    queue_status try_enqueue(U&& u) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (closed_) {
            return queue_status::CLOSED;
        } else if (queue_.size() >= capacity_) {
            return queue_status::FULL;
        }

        queue_.push(std::forward<U>(u));
        cv_.notify_one();
        return queue_status::SUCCESS;
    }

    // bool enqueue(const value_type& val) {
    //     std::lock_guard<std::mutex> lock(mtx_);
    // }

    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty() || closed_; });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T t = std::move(queue_.front());
        queue_.pop();
        return t;
    }

    std::optional<T> try_dequeue() {
        std::lock_guard<std::mutex> lock(mtx_);

        if (queue_.empty() || closed_) {
            return std::nullopt;
        }

        T t = std::move(queue_.front());
        queue_.pop();
        return t;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) {
                return;
            }
            closed_ = true;
        }
        cv_.notify_all();
    }

    bool is_closed() {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<T> empty_queue;
        queue_.swap(empty_queue);
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    size_t capacity() {
        return capacity;
    }

   private:
    const size_t capacity_{0};
    bool closed_{false};

    std::queue<T> queue_;
    std::condition_variable cv_;
    std::mutex mtx_;
};
