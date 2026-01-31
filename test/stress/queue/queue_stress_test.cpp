#include <gtest/gtest.h>

#include <atomic>
#include <vector>

#include "queue/lock_bounded_queue.h"
#include "queue/lock_free_bounded_queue.h"
#include "queue/lock_queue.h"
#include "queue/ms_queue.h"
#include "queue_factory.h"
#include "queue_stress_helpers.h"

template <typename T>
class queue_stress : public ::testing::Test {
   protected:
    void SetUp() override {
        using factory = queue_factory<T, 2048>;
        queue_ = factory::create();
        capacity_ = factory::capacity_;
    }

    std::unique_ptr<T> queue_;
    size_t capacity_{0};
};

using queue_impls = ::testing::Types<lock_queue<element_type>,
                                     lock_bounded_queue<element_type>,
                                     lock_free_bounded_queue<element_type>,
                                     ms_queue<element_type>>;

TYPED_TEST_SUITE(queue_stress, queue_impls);

TYPED_TEST(queue_stress, bounded_queue_fulness) {
    stress_test_config config = bounded_fulness_config();

    data_validator validator(config.producer_count, config.items_per_producer);
    barrier_sync sync(config.producer_count + config.consumer_count);
    std::atomic<size_t> producers_done{0};

    std::vector<std::future<void>> producers = launch_producers(*this->queue_, config, validator, sync, producers_done);
    std::vector<std::future<void>> consumers = launch_consumers(*this->queue_, config, validator, sync, producers_done);

    ASSERT_TRUE(wait_for_completion(producers, config.timeout_seconds))
        << "Producers timeout after " << config.timeout_seconds << " seconds";

    this->queue_->close();

    ASSERT_TRUE(wait_for_completion(consumers, config.timeout_seconds))
        << "Consumers timeout after " << config.timeout_seconds << " seconds";

    EXPECT_EQ(validator.total_produced(), config.producer_count * config.items_per_producer);
}

TYPED_TEST(queue_stress, push_pop_data_integrity) {
    stress_test_config config = push_pop_config();

    data_validator validator(config.producer_count, config.items_per_producer);
    barrier_sync sync(config.producer_count + config.consumer_count);
    std::atomic<size_t> producers_done{0};

    std::vector<std::future<void>> producers = launch_producers(*this->queue_, config, validator, sync, producers_done);
    std::vector<std::future<void>> consumers = launch_consumers(*this->queue_, config, validator, sync, producers_done);

    ASSERT_TRUE(wait_for_completion(producers, config.timeout_seconds))
        << "Producers timeout after " << config.timeout_seconds << " seconds";

    this->queue_->close();

    ASSERT_TRUE(wait_for_completion(consumers, config.timeout_seconds))
        << "Consumers timeout after " << config.timeout_seconds << " seconds";

    EXPECT_EQ(validator.total_produced(), config.producer_count * config.items_per_producer);
}

TYPED_TEST(queue_stress, dequeue_stress) {
    stress_test_config config = dequeue_stress_config();

    size_t item_count = std::min(config.items_per_producer, this->capacity_);
    fill_queue_to_count(*this->queue_, item_count);
    this->queue_->close();

    std::atomic<size_t> consumed_count{0};
    std::vector<std::future<void>> consumers = launch_consumers(*this->queue_, config, consumed_count);

    ASSERT_TRUE(wait_for_completion(consumers, config.timeout_seconds))
        << "Consumers timeout after " << config.timeout_seconds << " seconds";

    EXPECT_EQ(consumed_count.load(), item_count);
    EXPECT_TRUE(this->queue_->empty());
}
