#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class lock_bounded_queue {
   public:
    using value_type = T;

   public:
    explicit lock_bounded_queue(size_t capacity = 0) : capacity_(capacity) {}
    ~lock_bounded_queue() {
        close();
    }

    lock_bounded_queue(const lock_bounded_queue&) = delete;
    lock_bounded_queue(lock_bounded_queue&&) = delete;
    lock_bounded_queue& operator=(const lock_bounded_queue&) = delete;
    lock_bounded_queue& operator=(lock_bounded_queue&&) = delete;

    template <typename Func>
    bool try_enqueue_with(Func&& f) {
        return try_enqueue_impl([&f](std::queue<value_type>& queue) {
            value_type val;
            f(val);
            queue.push(std::move(val));
        });
    }

    bool try_enqueue(const value_type& val) {
        return try_enqueue_impl([&val](std::queue<value_type>& queue) { queue.push(val); });
    }

    bool try_enqueue(value_type&& val) {
        return try_enqueue_impl([&val](std::queue<value_type>& queue) { queue.push(std::move(val)); });
    }

    template <typename Func>
    bool enqueue_with(Func&& f) {
        return enqueue_impl([&f](std::queue<value_type>& queue) {
            value_type val;
            f(val);
            queue.push(std::move(val));
        });
    }

    bool enqueue(const value_type& val) {
        return enqueue_impl([&val](std::queue<value_type>& queue) { queue.push(val); });
    }

    bool enqueue(value_type&& val) {
        return enqueue_impl([&val](std::queue<value_type>& queue) { queue.push(std::move(val)); });
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        return enqueue_impl([&args...](std::queue<value_type>& queue) { queue.emplace(std::forward<Args>(args)...); });
    }

    template <typename Func>
    bool dequeue_with(Func&& f) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return !queue_.empty() || closed_; });

            if (queue_.empty()) {
                return false;
            }

            std::forward<Func>(f)(queue_.front());
            queue_.pop();
        }
        cv_.notify_one();
        return true;
    }

    template <typename Func>
    bool try_dequeue_with(Func&& f) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_ || queue_.empty()) {
                return false;
            }

            std::forward<Func>(f)(queue_.front());
            queue_.pop();
        }
        cv_.notify_one();
        return true;
    }

    bool dequeue(value_type& val) {
        return dequeue_with([&val](value_type& item) { val = std::move(item); });
    }

    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        bool success = dequeue_with([&result](value_type& item) { result.emplace(std::move(item)); });
        return success ? result : std::nullopt;
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

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    size_t capacity() const {
        return capacity_;
    }

    bool is_full() {
        std::lock_guard<std::mutex> lock(mtx_);
        return (capacity_ > 0) && (queue_.size() >= capacity_);
    }

   private:
    template <typename Operation>
    bool enqueue_impl(Operation&& op) {
        {
            std::unique_lock<std::mutex> lock(mtx_);

            cv_.wait(lock, [this]() { return queue_.size() < capacity_ || closed_; });

            if (closed_) {
                return false;
            }

            std::forward<Operation>(op)(queue_);
        }
        cv_.notify_one();
        return true;
    }

    template <typename Operation>
    bool try_enqueue_impl(Operation&& op) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_ || queue_.size() >= capacity_) {
                return false;
            }

            std::forward<Operation>(op)(queue_);
        }
        cv_.notify_one();
        return true;
    }

   private:
    const size_t capacity_{0};
    bool closed_{false};

    std::queue<T> queue_;
    std::condition_variable cv_;
    std::mutex mtx_;
};
