#include <gtest/gtest.h>

#include <future>
#include <vector>

#include "queue/lock_bounded_queue.h"
#include "queue/lock_free_bounded_queue.h"
#include "queue/lock_queue.h"
#include "queue/ms_queue.h"
#include "queue_factory.h"

template <typename T>
class queue_ut : public ::testing::Test {
   protected:
    using queue_type = T;
    using element_type = typename queue_type::value_type;

   protected:
    void SetUp() override {
        using factory = queue_factory<queue_type, 2048>;
        queue_ = factory::create();
        capacity_ = factory::capacity_;
    }
    std::unique_ptr<queue_type> queue_;
    size_t capacity_{0};
};

using queue_impls = ::testing::Types<lock_queue<uint32_t>, lock_bounded_queue<uint32_t>,
                                     lock_free_bounded_queue<uint32_t>, ms_queue<uint32_t>>;

TYPED_TEST_SUITE(queue_ut, queue_impls);

// ========== Basic State Tests ==========
TYPED_TEST(queue_ut, init_empty) {
    using queue_type = typename TestFixture::queue_type;

    queue_type& queue = *(this->queue_);

    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.empty(), true);
}

// ========== Enqueue Tests ==========
TYPED_TEST(queue_ut, enqueue_lvalue) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(in));
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());
}

TYPED_TEST(queue_ut, enqueue_rvalue) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(std::move(in)));
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());
}

TYPED_TEST(queue_ut, enqueue_with_lambda) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    auto f = [](element_type& dest) { dest = 42; };
    EXPECT_TRUE(queue.enqueue_with(f));
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());
}

TYPED_TEST(queue_ut, emplace) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    EXPECT_TRUE(queue.emplace(42));
    EXPECT_EQ(queue.size(), 1);

    element_type out;
    EXPECT_TRUE(queue.dequeue(out));
    EXPECT_EQ(out, 42);
}

TYPED_TEST(queue_ut, try_enqueue_with) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    auto f = [](element_type& dest) { dest = 42; };
    EXPECT_TRUE(queue.try_enqueue_with(f));
    EXPECT_EQ(queue.size(), 1);

    element_type out;
    EXPECT_TRUE(queue.dequeue(out));
    EXPECT_EQ(out, 42);
}

TYPED_TEST(queue_ut, try_enqueue_with_after_close) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    queue.close();
    auto f = [](element_type& dest) { dest = 42; };
    EXPECT_FALSE(queue.try_enqueue_with(f));
}

// ========== Dequeue Tests ==========
TYPED_TEST(queue_ut, dequeue_lvalue) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(in));

    element_type out;
    EXPECT_TRUE(queue.dequeue(out));
    EXPECT_EQ(out, in);
    EXPECT_EQ(queue.size(), 0);
}

TYPED_TEST(queue_ut, dequeue_rvalue) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(std::move(in)));

    element_type out;
    EXPECT_TRUE(queue.dequeue(out));
    EXPECT_EQ(out, in);
    EXPECT_EQ(queue.size(), 0);
}

TYPED_TEST(queue_ut, dequeue_with_lambda) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    auto fin = [&in](element_type& dest) { dest = in; };
    EXPECT_TRUE(queue.enqueue_with(fin));

    element_type out;
    auto fout = [&out](element_type& dest) { out = dest; };
    EXPECT_TRUE(queue.dequeue_with(fout));
    EXPECT_EQ(out, in);
}

TYPED_TEST(queue_ut, dequeue_optional) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(in));

    std::optional<element_type> out = queue.dequeue();
    EXPECT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), in);
}

TYPED_TEST(queue_ut, try_dequeue_with) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type value;
    EXPECT_FALSE(queue.try_dequeue_with([&value](element_type& src) { value = src; }));

    EXPECT_TRUE(queue.enqueue(42));
    EXPECT_TRUE(queue.try_dequeue_with([&value](element_type& src) { value = src; }));
    EXPECT_EQ(value, 42);

    EXPECT_FALSE(queue.try_dequeue_with([&value](element_type& src) { value = src; }));
}

TYPED_TEST(queue_ut, try_dequeue_with_after_close) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    queue.close();
    element_type value;
    EXPECT_FALSE(queue.try_dequeue_with([&value](element_type& src) { value = src; }));
}

// ========== Close Behavior Tests ==========
TYPED_TEST(queue_ut, is_closed) {
    using queue_type = typename TestFixture::queue_type;

    queue_type& queue = *(this->queue_);

    EXPECT_TRUE(queue.enqueue(42));
    EXPECT_FALSE(queue.is_closed());
    EXPECT_EQ(queue.size(), 1);

    queue.close();
    EXPECT_TRUE(queue.is_closed());
    EXPECT_EQ(queue.size(), 1);
}

TYPED_TEST(queue_ut, enqueue_closed) {
    using queue_type = typename TestFixture::queue_type;

    queue_type& queue = *(this->queue_);

    queue.close();
    EXPECT_FALSE(queue.enqueue(42));
    EXPECT_TRUE(queue.is_closed());
    EXPECT_EQ(queue.size(), 0);
}

TYPED_TEST(queue_ut, dequeue_closed) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    element_type in{42};
    EXPECT_TRUE(queue.enqueue(in));
    queue.close();

    element_type out;
    EXPECT_TRUE(queue.dequeue(out));
    EXPECT_EQ(out, in);
}

TYPED_TEST(queue_ut, dequeue_with_after_close) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    queue.close();
    element_type value;
    auto f = [&value](element_type& src) { value = src; };
    EXPECT_FALSE(queue.dequeue_with(f));
}

TYPED_TEST(queue_ut, dequeue_optional_empty) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    queue.close();
    std::optional<element_type> out = queue.dequeue();
    EXPECT_FALSE(out.has_value());
}

// ========== Sequential Bulk Tests ==========
TYPED_TEST(queue_ut, sequential_in_sequential_out) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    const size_t ITEM_NUM = TestFixture::capacity_;
    for (uint32_t i = 0; i < ITEM_NUM; i++) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }
    EXPECT_EQ(queue.size(), ITEM_NUM);
    EXPECT_FALSE(queue.empty());

    for (uint32_t i = 0; i < ITEM_NUM; i++) {
        element_type out;
        EXPECT_TRUE(queue.dequeue(out));
        EXPECT_EQ(out, static_cast<element_type>(i));
    }
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

TYPED_TEST(queue_ut, sequential_in_sequential_out_with_lambda) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    const size_t ITEM_NUM = TestFixture::capacity_;
    for (uint32_t i = 0; i < ITEM_NUM; i++) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }
    EXPECT_EQ(queue.size(), ITEM_NUM);
    EXPECT_FALSE(queue.empty());

    for (uint32_t i = 0; i < ITEM_NUM; i++) {
        element_type out;
        EXPECT_TRUE(queue.dequeue(out));
        EXPECT_EQ(out, static_cast<element_type>(i));
    }
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

// ========== Boundary Condition Tests ==========
TYPED_TEST(queue_ut, partial_fill) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    const size_t half_capacity = TestFixture::capacity_ / 2;
    for (size_t i = 0; i < half_capacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }

    EXPECT_EQ(queue.size(), half_capacity);
    EXPECT_FALSE(queue.empty());
}

TYPED_TEST(queue_ut, enqueue_dequeue_interleaved) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
        element_type out;
        EXPECT_TRUE(queue.dequeue(out));
        EXPECT_EQ(out, static_cast<element_type>(i));
    }

    EXPECT_TRUE(queue.empty());
}

// ========== Concurrent Tests ==========
template <typename QueueType>
std::vector<std::future<void>> create_producer_tasks(QueueType& queue, std::atomic<size_t>& count, size_t item_num,
                                                     size_t producer_num) {
    using element_type = typename QueueType::value_type;
    std::vector<std::future<void>> tasks;

    auto task = [&queue, &count, item_num](size_t task_id) {
        for (size_t i = 0; i < item_num; i++) {
            EXPECT_TRUE(queue.enqueue(static_cast<element_type>(item_num * task_id + i)));
            count.fetch_add(1);
        }
    };

    for (size_t i = 0; i < producer_num; i++) {
        tasks.emplace_back(std::async(std::launch::async, task, i));
    }

    return tasks;
}

template <typename QueueType>
std::vector<std::future<void>> create_consumer_tasks(QueueType& queue, std::atomic<size_t>& count, size_t item_num,
                                                     size_t consumer_num) {
    using element_type = typename QueueType::value_type;
    std::vector<std::future<void>> tasks;

    auto task = [&queue, &count]() {
        element_type out;
        while (queue.dequeue(out)) {
            count.fetch_add(1);
        }
    };

    for (uint32_t i = 0; i < consumer_num; i++) {
        tasks.emplace_back(std::async(std::launch::async, task));
    }

    return tasks;
}

template <typename QueueType>
void multi_in_multi_out_test(QueueType& queue, size_t item_num, size_t producer_num, size_t consumer_num) {
    std::atomic<size_t> produce_cnt{0};
    std::atomic<size_t> consume_cnt{0};
    std::vector<std::future<void>> producer_tasks = create_producer_tasks(queue, produce_cnt, item_num, producer_num);
    std::vector<std::future<void>> consumer_tasks = create_consumer_tasks(queue, consume_cnt, item_num, consumer_num);

    producer_tasks.clear();
    queue.close();
    consumer_tasks.clear();

    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(produce_cnt, consume_cnt);
}

TYPED_TEST(queue_ut, multi_in_multi_out) {
    using queue_type = typename TestFixture::queue_type;
    queue_type& queue = *(this->queue_);

    const size_t PRODUCER_NUM = 16;
    const size_t CONSUMER_NUM = 16;

    multi_in_multi_out_test(queue, TestFixture::capacity_, PRODUCER_NUM, CONSUMER_NUM);
}

TYPED_TEST(queue_ut, single_in_multi_out) {
    using queue_type = typename TestFixture::queue_type;
    queue_type& queue = *(this->queue_);

    const size_t PRODUCER_NUM = 1;
    const size_t CONSUMER_NUM = 16;

    multi_in_multi_out_test(queue, TestFixture::capacity_, PRODUCER_NUM, CONSUMER_NUM);
}

TYPED_TEST(queue_ut, multi_in_single_out) {
    using queue_type = typename TestFixture::queue_type;
    queue_type& queue = *(this->queue_);

    const size_t PRODUCER_NUM = 16;
    const size_t CONSUMER_NUM = 1;

    multi_in_multi_out_test(queue, TestFixture::capacity_, PRODUCER_NUM, CONSUMER_NUM);
}

TYPED_TEST(queue_ut, single_in_single_out) {
    using queue_type = typename TestFixture::queue_type;
    queue_type& queue = *(this->queue_);

    const size_t PRODUCER_NUM = 1;
    const size_t CONSUMER_NUM = 1;

    multi_in_multi_out_test(queue, TestFixture::capacity_, PRODUCER_NUM, CONSUMER_NUM);
}

// ========== Stress Test for Concurrent Dequeue ==========
// This test specifically targets the use-after-free race condition
// that can occur when multiple threads compete to dequeue nodes.
TYPED_TEST(queue_ut, concurrent_dequeue_stress) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    const size_t ITEM_NUM = 1000;
    const size_t CONSUMER_NUM = 32;

    // Enqueue all items first
    for (size_t i = 0; i < ITEM_NUM; i++) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }

    // Close the queue after enqueuing
    queue.close();

    std::atomic<size_t> consume_cnt{0};
    std::atomic<bool> has_error{false};
    std::vector<std::future<void>> consumer_tasks;

    // Create many consumers competing to dequeue
    for (size_t i = 0; i < CONSUMER_NUM; i++) {
        consumer_tasks.emplace_back(std::async(std::launch::async, [&]() {
            element_type out;
            size_t local_cnt = 0;
            while (queue.dequeue(out)) {
                local_cnt++;
                consume_cnt.fetch_add(1);
                // Sanity check: value should be less than ITEM_NUM
                if (out >= ITEM_NUM) {
                    has_error.store(true);
                }
            }
        }));
    }

    // Wait with timeout to detect hangs
    auto start = std::chrono::steady_clock::now();
    for (auto& task : consumer_tasks) {
        auto status = task.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::timeout) {
            FAIL() << "Test timed out - likely deadlock or infinite loop detected";
        }
    }

    EXPECT_EQ(consume_cnt, ITEM_NUM);
    EXPECT_FALSE(has_error.load());
    EXPECT_TRUE(queue.empty());
}
