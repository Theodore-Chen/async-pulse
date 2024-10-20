#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <future>

#include <queue/lock_queue.h>

TEST(LockQueueUt, InitEmpty) {
    LockQueue<uint32_t> lq;
    EXPECT_EQ(lq.Size(), 0);
    EXPECT_EQ(lq.Empty(), true);
}

TEST(LockQueueUt, InitUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    EXPECT_EQ(lq.Size(), 0);
    EXPECT_EQ(lq.Empty(), true);
}

TEST(LockQueueUt, EnqueueRvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.Enqueue(std::move(in));
    EXPECT_EQ(lq.Size(), 1);
    EXPECT_EQ(lq.Empty(), false);
}

TEST(LockQueueUt, EnqueueLvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.Enqueue(in);
    EXPECT_EQ(lq.Size(), 1);
    EXPECT_EQ(lq.Empty(), false);
}

TEST(LockQueueUt, DequeueRvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.Enqueue(std::move(in));
    uint32_t out;
    EXPECT_EQ(lq.Dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, DequeueLvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.Enqueue(in);
    uint32_t out;
    EXPECT_EQ(lq.Dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, EnqueueUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.Enqueue(std::move(in));
    EXPECT_EQ(lq.Size(), 1);
    EXPECT_EQ(lq.Empty(), false);
}

TEST(LockQueueUt, DequeueUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.Enqueue(std::move(in));
    std::unique_ptr<uint32_t> out;
    EXPECT_EQ(lq.Dequeue(out), true);
    EXPECT_EQ(*out, 42);
}

TEST(LockQueueUt, SingleInSingleOut) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    for (uint32_t i = 0; i < 1000; i++) {
        std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(i);
        lq.Enqueue(std::move(in));
    }
    for (uint32_t i = 0; i < 1000; i++) {
        std::unique_ptr<uint32_t> out;
        EXPECT_EQ(lq.Dequeue(out), true);
        EXPECT_EQ(*out, i);
    }
}

TEST(LockQueueUt, MultiInMultiOut) {
    using Info = std::pair<uint32_t, uint32_t>;
    auto inTask = [](uint32_t taskId, LockQueue<std::unique_ptr<Info>>& lq) -> void {
        for (uint32_t i = 0; i < 1000; i++) {
            std::unique_ptr<Info> in = std::make_unique<Info>(Info(taskId, i));
            lq.Enqueue(std::move(in));
        }
    };
    auto outTask = [](LockQueue<std::unique_ptr<Info>>& lq, std::vector<std::atomic<uint32_t>>& answer) -> void {
        std::unique_ptr<Info> info;
        while(lq.Dequeue(info)) {
            EXPECT_NE(info, nullptr);
            EXPECT_GE(info->first, 0);
            EXPECT_LT(info->first, 10);
            EXPECT_EQ(info->second, answer[info->first].load());
            (answer[info->first])++;
        }
    };
    LockQueue<std::unique_ptr<Info>> lq;
    for (uint32_t i = 0; i < 10; i++) {
        std::async(std::launch::async, inTask, i, std::ref(lq));
    }
    std::vector<std::atomic<uint32_t>> answer(10);
    for (uint32_t i = 0; i < 10; i++) {
        std::async(std::launch::async, outTask, std::ref(lq), std::ref(answer));
    }
}
