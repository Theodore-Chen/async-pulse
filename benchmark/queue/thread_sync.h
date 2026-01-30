#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace benchmark {

// 启动同步器：让所有工作线程等待主线程的启动信号
struct start_sync {
    std::mutex mtx_;
    std::condition_variable cv_;
    std::condition_variable all_ready_cv_;
    bool ready_{false};
    size_t waiting_count_{0};
    size_t expected_count_{0};

    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        ++waiting_count_;
        all_ready_cv_.notify_one();
        cv_.wait(lock, [this] { return ready_; });
    }

    void set_expected_count(size_t count) {
        std::lock_guard<std::mutex> lock(mtx_);
        expected_count_ = count;
    }

    void wait_until_all_ready() {
        std::unique_lock<std::mutex> lock(mtx_);
        all_ready_cv_.wait(lock, [this] { return waiting_count_ >= expected_count_; });
    }

    void notify_all() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            ready_ = true;
        }
        cv_.notify_all();
    }
};

} // namespace benchmark
