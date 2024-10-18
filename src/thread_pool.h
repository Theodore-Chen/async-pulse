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

template <typename UserData>
class ThreadPool {
   public:
    struct CalcData {
        UserData user;
        std::promise<UserData> prms;
        CalcData(UserData userData) : user(std::move(userData)) {}
        CalcData() {}
    };

    using CallBackFunc = std::function<void(UserData&)>;

    ThreadPool(CallBackFunc callback) : calcCallback_(callback) {
        if (calcCallback_) {
            ready_.store(true);
            for (uint32_t i = 0; i < THREAD_NUM; i++) {
                threads_.emplace_back(std::make_shared<std::thread>(&ThreadPool::ThreadTask, this));
            }
        }
    }

    ~ThreadPool() {
        ready_.store(false);
        for (auto& th : threads_) {
            th->join();
        }
    }

    std::future<UserData> Push(const UserData& userData) {
        if (!calcCallback_) {
            return std::future<UserData>();
        }
        std::shared_ptr<CalcData> data = std::make_shared<CalcData>(userData);
        std::lock_guard<std::mutex> ql(queLock_);
        queData_.push(data);
        return data->prms.get_future();
    }

   private:
    void ThreadTask() {
        auto next = [this]() -> std::shared_ptr<CalcData> {
            std::shared_ptr<CalcData> data = nullptr;
            std::lock_guard<std::mutex> ql(queLock_);
            if (!queData_.empty()) {
                data = queData_.front();
                queData_.pop();
            }
            return data;
        };
        std::shared_ptr<CalcData> data;
        while (ready_.load()) {
            if (data = next()) {
                calcCallback_(data->user);
                data->prms.set_value(std::move(data->user));
            }
        }
    }

   private:
    const size_t THREAD_NUM = 10;
    static const size_t THREAD_NUM_DEFAULT;
    static const size_t THREAD_NUM_MAX;
    CallBackFunc calcCallback_;
    std::vector<std::shared_ptr<std::thread>> threads_;
    std::atomic<bool> ready_{false};
    std::mutex queLock_;
    std::queue<std::shared_ptr<CalcData>> queData_;
};
