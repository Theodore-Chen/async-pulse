#include <gtest/gtest.h>
#include <vector>
#include "thread_pool_bind.h"

TEST(ThreadPoolBindUt, Create) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);
    EXPECT_EQ(threadPool.GetThreadNum(), threadPool.THREAD_NUM_DEFAULT);
}

TEST(ThreadPoolBindUt, Create2) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);
    EXPECT_EQ(threadPool.GetThreadNum(), threadPool.THREAD_NUM_DEFAULT);

    auto result = threadPool.Push([](int answer) { return answer; }, 42);
    EXPECT_EQ(result.get(), 42);
}

TEST(ThreadPoolBindUt, CreateMax) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_MAX);
    EXPECT_EQ(threadPool.GetThreadNum(), threadPool.THREAD_NUM_MAX);
}

TEST(ThreadPoolBindUt, CreateExceedMax) {
    ThreadPool threadPool(1000);
    EXPECT_EQ(threadPool.GetThreadNum(), threadPool.THREAD_NUM_MAX);
}

TEST(ThreadPoolBindUt, CreateZero) {
    ThreadPool threadPool(0);
    EXPECT_EQ(threadPool.Valid(), false);
    EXPECT_EQ(threadPool.GetThreadNum(), 0);

    std::future<int> result = threadPool.Push([](int answer) { return answer; }, 42);
    EXPECT_EQ(result.valid(), false);
}

TEST(ThreadPoolBindUt, SubmitTaskWithReturn) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);

    int answer = 42;
    std::future<int> result = threadPool.Push([](int ans) { return ans; }, answer);
    EXPECT_EQ(result.valid(), true);
    EXPECT_EQ(result.get(), answer);
}

TEST(ThreadPoolBindUt, SubmitTaskWithoutReturn) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);

    int answer = 0;
    auto taskWithoutReturn = [&answer](int ans) -> void { answer = ans; };
    std::future<void> result = threadPool.Push(taskWithoutReturn, 42);
    result.wait();
    EXPECT_EQ(result.valid(), true);
    EXPECT_EQ(answer, 42);
}

TEST(ThreadPoolBindUt, SubmitTaskByMultiThread) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);

    auto task = [&threadPool](int id) {
        for (int i = 0; i < 1000; i++) {
            auto result = threadPool.Push([](int a, int b) { return a * 10000 + b; }, id, i);
            EXPECT_EQ(result.valid(), true);
            EXPECT_EQ(result.get(), id * 10000 + i);
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(task, i);
    }
    for (auto& thd : threads) {
        thd.join();
    }
}

TEST(ThreadPoolBindUt, SubmitTask) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);

    auto task = [](int n) -> int { return n * 2; };
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 1000; i++) {
        futures.emplace_back(threadPool.Push(task, i));
    }
    for (int i = 0; i < futures.size(); i++) {
        EXPECT_EQ(futures[i].valid(), true);
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST(ThreadPoolBindUt, Destroy) {
    ThreadPool threadPool(ThreadPool::THREAD_NUM_DEFAULT);
    EXPECT_EQ(threadPool.Valid(), true);

    std::future<int> result = threadPool.Push([](int n) { return n; }, 42);
    EXPECT_EQ(result.valid(), true);
    EXPECT_EQ(result.get(), 42);

    threadPool.Destroy();
    EXPECT_EQ(threadPool.Valid(), false);
    EXPECT_EQ(threadPool.GetThreadNum(), 0);
}
