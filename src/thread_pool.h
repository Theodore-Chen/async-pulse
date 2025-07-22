#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "queue/lock_queue.h"

void ThreadPoolTest();

template <typename Data>
class ThreadPool {
   public:
    using PrmsData = std::pair<Data, std::promise<Data>>;
    using CallBack = std::function<void(Data&)>;
    static const size_t THREAD_NUM_DEFAULT = 4;
    static const size_t THREAD_NUM_MAX = 10;

   public:
    ThreadPool(size_t threadNum, CallBack callback);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) noexcept = default;
    ThreadPool& operator=(ThreadPool&&) noexcept = default;

    std::future<Data> Submit(Data&& data);
    bool Valid();
    void Destroy();
    size_t Size();
    size_t SizeDefault() const;
    size_t SizeMax() const;

   private:
    static void ThreadTask(ThreadPool* tp);

   private:
    CallBack callback_{nullptr};
    std::vector<std::thread> threads_;
    std::atomic<bool> ready_{false};
    lock_queue<PrmsData> queue_;
};

template <typename Data>
inline ThreadPool<Data>::ThreadPool(size_t threadNum, CallBack callback) : callback_(callback) {
    if (callback_) {
        size_t num = threadNum > THREAD_NUM_MAX ? THREAD_NUM_MAX : threadNum;
        ready_.store(num == 0 ? false : true);
        for (uint32_t i = 0; i < num; i++) {
            threads_.emplace_back(ThreadTask, this);
        }
    }
}

template <typename Data>
ThreadPool<Data>::~ThreadPool() {
    Destroy();
}

template <typename Data>
inline std::future<Data> ThreadPool<Data>::Submit(Data&& data) {
    if (!callback_) {
        return std::future<Data>();
    }
    PrmsData prmsData(std::forward<Data>(data), std::promise<Data>());
    std::future<Data> result = prmsData.second.get_future();
    queue_.enqueue(std::move(prmsData));
    return result;
}

template <typename Data>
void ThreadPool<Data>::ThreadTask(ThreadPool* tp) {
    while (tp->ready_.load()) {
        PrmsData data;
        if (tp->queue_.dequeue(data)) {
            tp->callback_(data.first);
            data.second.set_value(std::move(data.first));
        }
    }
}

template <typename Data>
inline void ThreadPool<Data>::Destroy() {
    ready_.store(false);
    for (auto& th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    threads_.clear();
}

template <typename Data>
inline size_t ThreadPool<Data>::Size() {
    return threads_.size();
}

template <typename Data>
inline bool ThreadPool<Data>::Valid() {
    return threads_.size() != 0 && ready_.load() == true;
}

template <typename Data>
inline size_t ThreadPool<Data>::SizeDefault() const {
    return THREAD_NUM_DEFAULT;
}

template <typename Data>
inline size_t ThreadPool<Data>::SizeMax() const {
    return THREAD_NUM_MAX;
}
