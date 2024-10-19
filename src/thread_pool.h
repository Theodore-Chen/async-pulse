#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

void ThreadPoolTest();

template <typename Data>
class ThreadPool {
   public:
    using PrmsData = std::pair<Data, std::promise<Data>>;
    using CallBackFunc = std::function<void(Data&)>;

    ThreadPool(size_t thread_num, CallBackFunc callback) : calcCallback_(callback) {
        if (calcCallback_) {
            size_t num = thread_num > THREAD_NUM_MAX ? THREAD_NUM_MAX : thread_num;
            ready_.store(num == 0 ? false : true);
            for (uint32_t i = 0; i < num; i++) {
                threads_.emplace_back(ThreadTask, this);
            }
        }
    }

    ~ThreadPool() { Destroy(); }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;

    std::future<Data> Submit(Data&& data) {
        if (!calcCallback_) {
            return std::future<Data>();
        }
        PrmsData prmsData(std::forward<Data>(data), std::promise<Data>());
        std::future<Data> result = prmsData.second.get_future();
        std::lock_guard<std::mutex> ql(queLock_);
        queData_.push(std::move(prmsData));
        return result;
    }

    size_t Size() { return threads_.size(); }

    bool Valid() { return threads_.size() != 0 && ready_.load() == true; }

    void Destroy() {
        ready_.store(false);
        for (auto& th : threads_) {
            if (th.joinable()) {
                th.join();
            }
        }
        threads_.clear();
    }

    size_t SizeDefault() const { return THREAD_NUM_DEFAULT; }

    size_t SizeMax() const { return THREAD_NUM_MAX; }

   private:
    static void ThreadTask(ThreadPool* tp) {
        auto next = [tp](PrmsData& data) -> bool {
            std::lock_guard<std::mutex> ql(tp->queLock_);
            if (!tp->queData_.empty()) {
                data = std::move(tp->queData_.front());
                tp->queData_.pop();
                return true;
            }
            return false;
        };
        while (tp->ready_.load()) {
            PrmsData data;
            if (next(data)) {
                tp->calcCallback_(data.first);
                data.second.set_value(std::move(data.first));
            }
        }
    }

   public:
    static const size_t THREAD_NUM_DEFAULT = 4;
    static const size_t THREAD_NUM_MAX = 10;

   private:
    CallBackFunc calcCallback_{nullptr};
    std::vector<std::thread> threads_;
    std::atomic<bool> ready_{false};
    std::mutex queLock_;
    std::queue<PrmsData> queData_;
};
