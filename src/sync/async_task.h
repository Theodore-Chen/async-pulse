#include <iostream>
#include <future>
#include <thread>
#include <unordered_map>

class AsyncTask {
   public:
    using TaskId = uint32_t;
    AsyncTask() = default;
    AsyncTask(const AsyncTask&) = delete;
    AsyncTask& operator=(const AsyncTask&) = delete;
    ~AsyncTask() {
        for (auto& task : mTasks_) {
            task.second.wait();
        }
    }

    AsyncTask& RunAsync(TaskId id) {
        if (mTasks_.find(id) != mTasks_.end()) {
            return *this;
        }
        std::promise<void> p;
        std::shared_future<void> sf(p.get_future().share());
        mTasks_.emplace(id, std::move(sf));
        AssignAsyncTask(id);
        p.set_value();
        return *this;
    }
    bool OnRunDone(TaskId id) {
        auto it = mTasks_.find(id);
        if (it != mTasks_.end()) {
            it->second.wait();
            AsyncTaskFinish(id);
            mTasks_.erase(it);
            return true;
        }
        return false;
    }

   private:
    std::unordered_map<TaskId, std::shared_future<void>> mTasks_;
    void AssignAsyncTask(TaskId id) {
        std::cout << "AssignAsyncTask, TaskId = " << id <<  std::endl;
        // std::this_thread::sleep_for(std::chrono::milliseconds(100 - id));
    }
    void AsyncTaskFinish(TaskId id) {
        std::cout << "AsyncTaskFinish, TaskId = " << id <<  std::endl;
    }
};
