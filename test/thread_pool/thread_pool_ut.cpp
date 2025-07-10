#include <gtest/gtest.h>
#include "thread_pool.h"

struct UtTestData {
    uint32_t in = 0;
    uint32_t out = 0;
};

void UtTestFunc(UtTestData& data) {
    data.out = data.in * 2;
};

void UtTestFuncHeavy(UtTestData& data) {
    volatile uint32_t a = 0;
    for (size_t i = 0; i < 10000; i++) {
        a += 1;
    }
    data.out = data.in * 2;
};

TEST(ThreadPoolUt, Create) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFunc);
    EXPECT_EQ(threadPool.Size(), threadPool.SizeDefault());
    EXPECT_EQ(threadPool.Valid(), true);
}

TEST(ThreadPoolUt, CreateMax) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_MAX, UtTestFunc);
    EXPECT_EQ(threadPool.Size(), threadPool.SizeMax());
    EXPECT_EQ(threadPool.Valid(), true);
}

TEST(ThreadPoolUt, CreateExceedMax) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_MAX + 1, UtTestFunc);
    EXPECT_EQ(threadPool.Size(), threadPool.SizeMax());
    EXPECT_EQ(threadPool.Valid(), true);
}

TEST(ThreadPoolUt, CreateInvalid) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, nullptr);
    EXPECT_EQ(threadPool.Valid(), false);
    EXPECT_EQ(threadPool.Size(), 0);
}

TEST(ThreadPoolUt, Submit) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFunc);
    std::future<UtTestData> handle = threadPool.Submit({1, 0});
    UtTestData data = handle.get();
    EXPECT_EQ(data.in, 1);
    EXPECT_EQ(data.out, 1 * 2);
}

TEST(ThreadPoolUt, SubmitInvalid) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, nullptr);
    std::future<UtTestData> handle = threadPool.Submit({1, 0});
    EXPECT_EQ(handle.valid(), false);
}

TEST(ThreadPoolUt, SubmitTasks) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFunc);
    std::vector<std::future<UtTestData>> handles;
    for (uint32_t i = 0; i < 1000; i++) {
        handles.emplace_back(threadPool.Submit({i, 0}));
    }
    for (size_t i = 0; i < 1000; i++) {
        UtTestData data = handles[i].get();
        EXPECT_EQ(data.in, i);
        EXPECT_EQ(data.out, i * 2);
    }
}

TEST(ThreadPoolUt, SubmitHeavyTasks) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFuncHeavy);
    std::vector<std::future<UtTestData>> handles;
    for (uint32_t i = 0; i < 1000; i++) {
        handles.emplace_back(threadPool.Submit({i, 0}));
    }
    for (uint32_t i = 0; i < 1000; i++) {
        UtTestData data = handles[i].get();
        EXPECT_EQ(data.in, i);
        EXPECT_EQ(data.out, i * 2);
    }
}

TEST(ThreadPoolUt, SubmitByMultiThread) {
    auto task = [](ThreadPool<UtTestData>& tp, uint32_t id) {
        for (uint32_t i = 0; i < 1000; i++) {
            uint32_t inPut = id * 10000 + i;
            std::future<UtTestData> handle = tp.Submit({inPut, 0});
            UtTestData data = handle.get();
            EXPECT_EQ(data.in, inPut);
            EXPECT_EQ(data.out, inPut * 2);
        }
    };
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFunc);
    std::vector<std::future<void>> threads;
    for (uint32_t i = 0; i < 10; i++) {
        threads.emplace_back(std::async(std::launch::async, task, std::ref(threadPool), i));
    }
}

TEST(ThreadPoolUt, SubmitByMultiThreadHeavy) {
    auto task = [](ThreadPool<UtTestData>& tp, uint32_t id) {
        for (uint32_t i = 0; i < 500; i++) {
            uint32_t inPut = id * 10000 + i;
            std::future<UtTestData> handle = tp.Submit({inPut, 0});
            UtTestData data = handle.get();
            EXPECT_EQ(data.in, inPut);
            EXPECT_EQ(data.out, inPut * 2);
        }
    };
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_DEFAULT, UtTestFuncHeavy);
    std::vector<std::future<void>> threads;
    for (uint32_t i = 0; i < 10; i++) {
        threads.emplace_back(std::async(std::launch::async, task, std::ref(threadPool), i));
    }
}

TEST(ThreadPoolUt, Destroy) {
    ThreadPool<UtTestData> threadPool(ThreadPool<UtTestData>::THREAD_NUM_MAX + 1, UtTestFunc);
    threadPool.Destroy();
    EXPECT_EQ(threadPool.Valid(), false);
    EXPECT_EQ(threadPool.Size(), 0);
}
