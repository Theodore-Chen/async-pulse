#include <gtest/gtest.h>
#include <queue/lock_queue.h>

#include <future>
#include <thread>
#include <vector>

TEST(LockQueueUt, InitEmpty) {
    lock_queue<uint32_t> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockQueueUt, InitUnmovable) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
    EXPECT_EQ(lq.size(), 0);
    EXPECT_EQ(lq.empty(), true);
}

TEST(LockQueueUt, EnqueueRvalue) {
    lock_queue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, EnqueueLvalue) {
    lock_queue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, DequeueRvalue) {
    lock_queue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(std::move(in));
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, DequeueLvalue) {
    lock_queue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    uint32_t out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(out, in);
}

TEST(LockQueueUt, EnqueueUnmovable) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, DequeueUnmovable) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    std::unique_ptr<uint32_t> out;
    EXPECT_EQ(lq.dequeue(out), true);
    EXPECT_EQ(*out, 42);
}

TEST(LockQueueUt, EnqueueClose) {
    lock_queue<uint32_t> lq;
    lq.close();
    EXPECT_TRUE(lq.is_closed());
    EXPECT_FALSE(lq.enqueue(42));
    EXPECT_EQ(lq.size(), 0);
}

TEST(LockQueueUt, DequeueClose) {
    lock_queue<uint32_t> lq;

    auto consumer_task = [&lq]() {
        uint32_t out;
        EXPECT_FALSE(lq.dequeue(out));
    };
    auto fut = std::async(std::launch::async, consumer_task);

    lq.close();
    fut.wait();
}

TEST(LockQueueUt, Clear) {
    lock_queue<uint32_t> lq;
    lq.enqueue(1);
    lq.enqueue(2);
    ASSERT_EQ(lq.size(), 2);

    lq.clear();
    EXPECT_EQ(lq.size(), 0);
    EXPECT_TRUE(lq.empty());

    lq.enqueue(3);
    EXPECT_EQ(lq.size(), 1);

    uint32_t out;
    EXPECT_TRUE(lq.dequeue(out));
    EXPECT_EQ(out, 3);
}

TEST(LockQueueUt, DestructorWakesUpConsumer) {
    std::unique_ptr<lock_queue<uint32_t>> lq_ptr = std::make_unique<lock_queue<uint32_t>>();
    std::promise<void> ready_promise;
    std::future<void> ready_future = ready_promise.get_future();

    auto consumer_task = [&lq_ptr](std::promise<void> p) {
        p.set_value();
        uint32_t out;
        EXPECT_FALSE(lq_ptr->dequeue(out));
    };
    auto fut = std::async(std::launch::async, consumer_task, std::move(ready_promise));

    ready_future.wait();
    lq_ptr.reset();
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
}

TEST(LockQueueUt, SingleInSingleOut) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
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
    const size_t PRODUCER_NUM = 10;
    const size_t CONSUMER_NUM = 10;
    const size_t INFO_NUM = 10000;

    using Info = std::pair<uint32_t, uint32_t>;
    lock_queue<std::unique_ptr<Info>> lq;

    std::vector<std::future<void>> producer_threads;
    std::vector<std::future<void>> consumer_threads;

    std::vector<std::vector<uint32_t>> received_orders(PRODUCER_NUM);
    for (auto& vec : received_orders) {
        vec.resize(INFO_NUM, UINT32_MAX);
    }

    auto producer_task = [&lq](uint32_t taskId) {
        for (uint32_t i = 0; i < INFO_NUM; i++) {
            lq.enqueue(std::make_unique<Info>(taskId, i));
        }
    };

    auto consumer_task = [&lq, &received_orders]() {
        std::unique_ptr<Info> info;
        while (lq.dequeue(info)) {
            ASSERT_NE(info, nullptr);
            ASSERT_LT(info->first, received_orders.size());
            ASSERT_LT(info->second, received_orders[info->first].size());
            received_orders[info->first][info->second] = info->second;
        }
    };

    for (uint32_t i = 0; i < PRODUCER_NUM; i++) {
        producer_threads.emplace_back(std::async(std::launch::async, producer_task, i));
    }
    for (uint32_t i = 0; i < CONSUMER_NUM; i++) {
        consumer_threads.emplace_back(std::async(std::launch::async, consumer_task));
    }
    for (auto& t : producer_threads) {
        t.wait();
    }
    lq.close();
    for (auto& t : consumer_threads) {
        t.wait();
    }

    for (uint32_t task_id = 0; task_id < PRODUCER_NUM; task_id++) {
        const auto& results = received_orders[task_id];

        ASSERT_EQ(results.size(), INFO_NUM);

        for (uint32_t i = 0; i < INFO_NUM; i++) {
            ASSERT_NE(results[i], UINT32_MAX) << "Missing value for task " << task_id << " seq " << i;
            EXPECT_EQ(results[i], i) << "Invalid value for task " << task_id << " seq " << i;
        }
    }
}
