#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class lock_queue {
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

    int size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    template <typename Arg>
    bool enqueue(Arg&& t) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (closed_ == true) {
                return false;
            }
            queue_.push(std::forward<Arg>(t));
        }
        cond_.notify_one();
        return true;
    }

    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this]() { return !queue_.empty() || closed_; });

        if (queue_.empty() && closed_) {
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
        cond_.notify_all();
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

   private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cond_;
    bool closed_{false};
};
