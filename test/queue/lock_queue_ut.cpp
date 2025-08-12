#include <gtest/gtest.h>

#include <future>
#include <thread>
#include <vector>

#include "queue/lock_bounded_queue.h"
#include "queue/lock_queue.h"

using lock_queue_t = lock_queue<uint32_t>;
using lock_queue_uncopyable_t = lock_queue<std::unique_ptr<uint32_t>>;
using bounded_queue_t = lock_bounded_queue<uint32_t>;
using bounded_queue_uncopyable_t = lock_bounded_queue<std::unique_ptr<uint32_t>>;

using queue_impls = ::testing::Types<lock_queue_t, bounded_queue_t>;

template <typename T>
struct queue_fectory;

template <typename T>
struct queue_fectory<lock_queue<T>> {
    static std::unique_ptr<lock_queue<T>> create() {
        return std::make_unique<lock_queue<T>>();
    }
};

template <typename T>
struct queue_fectory<lock_bounded_queue<T>> {
    static std::unique_ptr<lock_bounded_queue<T>> create() {
        constexpr size_t capacity = 2048;
        return std::make_unique<lock_bounded_queue<T>>(capacity, capacity * 0.8, capacity * 0.2);
    }
};

template <typename T>
class lock_queue_ut : public ::testing::Test {
   protected:
    void SetUp() override {
        queue_ = queue_fectory<T>::create();
    }
    std::unique_ptr<T> queue_;
};

TYPED_TEST_SUITE(lock_queue_ut, queue_impls);

TYPED_TEST(lock_queue_ut, init_empty) {
    EXPECT_EQ(this->queue_->size(), 0);
    EXPECT_EQ(this->queue_->empty(), true);
}

// TYPED_TEST(lock_queue_ut, enqueue_lvalue) {
//     uint32_t in = 42;
//     this->queue_->enqueue(in);
//     EXPECT_EQ(this->queue_.size(), 1);
//     EXPECT_FALSE(this->queue_.empty());
// }

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
    std::optional<uint32_t> out = lq.dequeue();
    EXPECT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), in);
}

TEST(LockQueueUt, DequeueLvalue) {
    lock_queue<uint32_t> lq;
    uint32_t in = 10;
    lq.enqueue(in);
    std::optional<uint32_t> out = lq.dequeue();
    EXPECT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), in);
}

TEST(LockQueueUt, EnqueueUncopyable) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    EXPECT_EQ(lq.size(), 1);
    EXPECT_EQ(lq.empty(), false);
}

TEST(LockQueueUt, DequeueUncopyable) {
    lock_queue<std::unique_ptr<uint32_t>> lq;
    std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(42);
    lq.enqueue(std::move(in));
    std::optional<std::unique_ptr<uint32_t>> out = lq.dequeue();
    EXPECT_TRUE(out.has_value());
    EXPECT_EQ(**out, 42);
}

TEST(LockQueueUt, IsClosed) {
    lock_queue<uint32_t> lq;
    lq.close();
    EXPECT_TRUE(lq.is_closed());
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
        std::optional<uint32_t> out = lq.dequeue();
        EXPECT_FALSE(out.has_value());
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

    std::optional<uint32_t> out = lq.dequeue();
    EXPECT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 3);
}

TEST(LockQueueUt, DestructorWakesUpConsumer) {
    std::unique_ptr<lock_queue<uint32_t>> lq_ptr = std::make_unique<lock_queue<uint32_t>>();
    std::promise<void> ready_promise;
    std::future<void> ready_future = ready_promise.get_future();

    auto consumer_task = [&lq_ptr](std::promise<void> p) {
        p.set_value();
        std::optional<uint32_t> out = lq_ptr->dequeue();
        EXPECT_FALSE(out.has_value());
    };
    auto fut = std::async(std::launch::async, consumer_task, std::move(ready_promise));

    ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    lq_ptr.reset();
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(50)), std::future_status::ready);
}

TEST(LockQueueUt, SingleInSingleOut) {
    const size_t INFO_NUM = 10000;
    lock_queue<std::unique_ptr<uint32_t>> lq;
    for (uint32_t i = 0; i < INFO_NUM; i++) {
        std::unique_ptr<uint32_t> in = std::make_unique<uint32_t>(i);
        lq.enqueue(std::move(in));
    }
    for (uint32_t i = 0; i < INFO_NUM; i++) {
        std::optional<std::unique_ptr<uint32_t>> out = lq.dequeue();
        EXPECT_TRUE(out.has_value());
        EXPECT_EQ(**out, i);
    }
}

TEST(LockQueueUt, MultiInMultiOut) {
    const size_t PRODUCER_NUM = 10;
    const size_t CONSUMER_NUM = 10;
    const size_t INFO_NUM = 10000;

    using Item = std::pair<uint32_t, uint32_t>;
    lock_queue<std::unique_ptr<Item>> lq;

    std::vector<std::future<void>> producer_threads;
    std::vector<std::future<void>> consumer_threads;

    std::vector<std::vector<uint32_t>> received_orders(PRODUCER_NUM);
    for (auto& vec : received_orders) {
        vec.resize(INFO_NUM, UINT32_MAX);
    }

    auto producer_task = [&lq](uint32_t taskId) {
        for (uint32_t i = 0; i < INFO_NUM; i++) {
            lq.enqueue(std::make_unique<Item>(taskId, i));
        }
    };

    auto consumer_task = [&lq, &received_orders]() {
        while (std::optional<std::unique_ptr<Item>> out = lq.dequeue()) {
            Item item = std::move(**out);
            ASSERT_LT(item.first, received_orders.size());
            ASSERT_LT(item.second, received_orders[item.first].size());
            received_orders[item.first][item.second] = item.second;
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
