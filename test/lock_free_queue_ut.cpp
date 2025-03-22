#include <gtest/gtest.h>
#include <atomic>
#include <future>
#include <vector>

#include <queue/lock_free_queue.h>

TEST(LockFreeQueueUt, InitEmpty) {
    LockFreeQueue<uint32_t, 1024> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockFreeQueueUt, InitUnmovable) {
    LockFreeQueue<std::unique_ptr<uint32_t>, 1024> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockFreeQueueUt, EnqueueRvalue) {
    LockFreeQueue<uint32_t, 1024> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockFreeQueueUt, EnqueueLvalue) {
    LockFreeQueue<uint32_t, 1024> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockFreeQueueUt, DequeueRvalue) {
    LockFreeQueue<uint32_t, 1024> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockFreeQueueUt, DequeueLvalue) {
    LockFreeQueue<uint32_t, 1024> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockFreeQueueUt, EnqueueUnmovable) {
    LockFreeQueue<std::unique_ptr<uint32_t>, 1024> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockFreeQueueUt, DequeueUnmovable) {
    LockFreeQueue<std::unique_ptr<uint32_t>, 1024> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    std::unique_ptr<uint32_t> out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(*out, 42);
}

TEST(LockFreeQueueUt, SingleInSingleOut) {
    LockFreeQueue<std::unique_ptr<uint32_t>, 1024> lq;
    for (uint32_t i = 0; i < 1000; i++) {
        std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(i);
        lq.enqueue(std::move(in));
    }
    for (uint32_t i = 0; i < 1000; i++) {
        std::unique_ptr<uint32_t> out;
        EXPECT_EQ(lq.dequeue(out), true);
        EXPECT_EQ(*out, i);
    }
}

TEST(LockFreeQueueUt, MultiInMultiOut) {
    const size_t TASK_NUM = 10;
    const size_t INFO_NUM = 65536;

    using TaskId = uint32_t;
    using InfoId = uint32_t;
    using Info = std::pair<TaskId, InfoId>;
    LockFreeQueue<Info, 1024> lq;
    std::vector<std::atomic<TaskId>> answer(TASK_NUM);
    std::vector<std::future<void>> threads;

    auto inTask = [&lq](TaskId taskId) -> void {
        for (InfoId i = 0; i < INFO_NUM; i++) {
            Info info(taskId, i);
            lq.enqueue(info);
        }
    };
    for (TaskId taskId = 0; taskId < TASK_NUM; taskId++) {
        threads.emplace_back(std::async(std::launch::async, inTask, taskId));
    }

    auto outTask = [&lq, &answer]() -> void {
        Info info;
        while (lq.dequeue(info)) {
            EXPECT_EQ(info.second, answer.at(info.first).load());
            (answer[info.first])++;
        }
    };
    for (TaskId taskId = 0; taskId < TASK_NUM; taskId++) {
        threads.emplace_back(std::async(std::launch::async, outTask));
    }
}
