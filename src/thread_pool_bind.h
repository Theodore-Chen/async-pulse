#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
   public:
    explicit ThreadPool(size_t thread_num = THREAD_NUM_DEFAULT);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;
    ~ThreadPool();

    void Destroy();
    size_t GetThreadNum();
    bool Valid();
    template <typename F, typename... Args>
    auto Push(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

   private:
    static void ThreadTask(ThreadPool* thread_pool);

   public:
    static const size_t THREAD_NUM_DEFAULT;
    static const size_t THREAD_NUM_MAX;

   private:
    using Task = std::function<void()>;
    std::vector<std::thread> threads_;
    std::queue<Task> tasks_;
    std::mutex task_mtx_;
    std::condition_variable task_cv_;
    std::atomic<bool> stop_{false};
};

template <typename F, typename... Args>
auto ThreadPool::Push(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    if (Valid() != true) {
        return std::future<decltype(f(args...))>();
    }

    using return_type = decltype(f(args...))();
    std::function<return_type> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto func_ptr = std::make_shared<std::packaged_task<return_type>>(func);
    Task task = [func_ptr]() { (*func_ptr)(); };

    {
        std::unique_lock<std::mutex> lock(task_mtx_);
        tasks_.emplace(task);
    }

    task_cv_.notify_one();
    return func_ptr->get_future();
}
