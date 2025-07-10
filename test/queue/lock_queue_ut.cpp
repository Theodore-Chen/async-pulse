#include <gtest/gtest.h>
#include <queue/lock_queue.h>

#include <atomic>
#include <future>
#include <vector>

TEST(LockQueueUt, InitEmpty) {
    LockQueue<uint32_t> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockQueueUt, InitUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockQueueUt, EnqueueRvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, EnqueueLvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, DequeueRvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, DequeueLvalue) {
    LockQueue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, EnqueueUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, DequeueUnmovable) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    std::unique_ptr<uint32_t> out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(*out, 42);
}

TEST(LockQueueUt, SingleInSingleOut) {
    LockQueue<std::unique_ptr<uint32_t>> lq;
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

TEST(LockQueueUt, MultiInMultiOut) {
    const size_t TASK_NUM = 10;
    const size_t INFO_NUM = 10000;

    using Info = std::pair<uint32_t, uint32_t>;
    LockQueue<std::unique_ptr<Info>> lq;
    std::vector<std::atomic<uint32_t>> answer(TASK_NUM);
    std::vector<std::future<void>> threads;
    std::atomic<uint32_t> enqueCnt(0);

    auto inTask = [&lq, &enqueCnt](uint32_t taskId) -> void {
        for (uint32_t i = 0; i < INFO_NUM; i++) {
            lq.enqueue(std::make_unique<Info>(Info(taskId, i)));
            enqueCnt++;
        }
    };
    for (uint32_t i = 0; i < TASK_NUM; i++) {
        threads.emplace_back(std::async(std::launch::async, inTask, i));
    }

    auto outTask = [&lq, &answer, &enqueCnt]() -> void {
        std::unique_ptr<Info> info;
        while (true) {
            if (lq.dequeue(info)) {
                EXPECT_NE(info, nullptr);
                answer[info->first] += info->second;
            } else if (enqueCnt.load() == TASK_NUM * INFO_NUM) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
    };
    for (uint32_t i = 0; i < TASK_NUM; i++) {
        threads.emplace_back(std::async(std::launch::async, outTask));
    }

    for (auto& t : threads) {
        t.wait();
    }
    for (uint32_t taskId = 0; taskId < TASK_NUM; taskId++) {
        constexpr uint32_t sum = (0 + INFO_NUM - 1) * INFO_NUM / 2;
        EXPECT_EQ(answer[taskId].load(), sum);
    }
}
