#include "thread_pool_bind.h"

const size_t ThreadPool::THREAD_NUM_DEFAULT = 4;
const size_t ThreadPool::THREAD_NUM_MAX = 10;

ThreadPool::ThreadPool(size_t thread_num) {
    size_t num = thread_num > THREAD_NUM_MAX ? THREAD_NUM_MAX : thread_num;
    stop_.store(thread_num == 0 ? true : false);
    for (size_t i = 0; i < num; ++i) {
        threads_.emplace_back(ThreadTask, this);
    }
}

void ThreadPool::ThreadTask(ThreadPool* thread_pool) {
    auto next = [thread_pool](Task& task) -> bool {
        task = nullptr;
        std::unique_lock<std::mutex> lock(thread_pool->task_mtx_);
        thread_pool->task_cv_.wait(lock, [thread_pool] { return thread_pool->stop_ || !thread_pool->tasks_.empty(); });
        if (!thread_pool->stop_ && !thread_pool->tasks_.empty()) {
            task = std::move(thread_pool->tasks_.front());
            thread_pool->tasks_.pop();
        }
        return static_cast<bool>(task);
    };
    Task task;
    while (!thread_pool->stop_) {
        if (next(task)) {
            task();
        }
    }
}

size_t ThreadPool::GetThreadNum() {
    return threads_.size();
}

bool ThreadPool::Valid() {
    return threads_.size() != 0 && stop_ != true;
}

void ThreadPool::Destroy() {
    stop_.store(true);
    task_cv_.notify_all();
    for (std::thread& thd : threads_) {
        if (thd.joinable()) {
            thd.join();
        }
    }
    threads_.clear();
}

ThreadPool::~ThreadPool() {
    Destroy();
}
