#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class lock_queue {
   public:
    using value_type = T;

   public:
    lock_queue() {}
    ~lock_queue() {
        close();
        clear();
    }
    lock_queue(const lock_queue&) = delete;
    lock_queue& operator=(const lock_queue&) = delete;
    lock_queue(lock_queue&&) = delete;
    lock_queue& operator=(lock_queue&&) = delete;

    bool empty() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
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
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this]() { return !queue_.empty() || closed_; });

        if (queue_.empty() && closed_) {
            return false;
        }

        std::forward<Func>(f)(queue_.front());
        queue_.pop();
        return true;
    }

    bool dequeue(value_type& val) {
        return dequeue_with([&val](value_type& item) { val = std::move(item); });
    }

    std::optional<value_type> dequeue() {
        std::optional<value_type> result;
        auto f = [&result](value_type& val) { result.emplace(std::move(val)); };
        return dequeue_with(f) ? std::move(result) : std::nullopt;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) {
                return;
            }
            closed_ = true;
        }
        cond_.notify_all();
    }

    bool is_closed() {
        std::lock_guard<std::mutex> lock(mtx_);
        return closed_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<value_type> empty_queue;
        queue_.swap(empty_queue);
    }

   private:
    template <typename Operation>
    bool enqueue_impl(Operation&& op) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_) {
                return false;
            }

            std::forward<Operation>(op)(queue_);
        }
        cond_.notify_one();
        return true;
    }

   private:
    std::queue<value_type> queue_;
    std::mutex mtx_;
    std::condition_variable cond_;
    bool closed_{false};
};
