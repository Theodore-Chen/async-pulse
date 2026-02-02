#include <gtest/gtest.h>

#include <vector>

#include "queue/lock_bounded_queue.h"
#include "queue/lock_free_bounded_queue.h"
#include "queue/lock_queue.h"
#include "queue/ms_queue.h"
#include "queue_factory.h"
#include "queue_stress_helpers.h"

using detail::ms_queue;

constexpr size_t QUEUE_CAPACITY = 2048;

template <typename T>
class queue_stress : public ::testing::Test {
   protected:
    void SetUp() override {
        queue_ = queue_factory<T, QUEUE_CAPACITY>::create();
    }

    std::unique_ptr<T> queue_;
};

using queue_impls = ::testing::Types<lock_queue<element_type>,
                                     lock_bounded_queue<element_type>,
                                     lock_free_bounded_queue<element_type>,
                                     ms_queue<element_type>>;

TYPED_TEST_SUITE(queue_stress, queue_impls);

template <typename Queue>
void run_mpmc_test(Queue& queue, const stress_test_config& config, data_validator& validator) {
    barrier_sync barrier(config.producer_count + config.consumer_count);
    std::atomic<size_t> producers_done{0};
    sync_context ctx{&validator, &barrier, &producers_done, config.producer_count};

    auto producers = launch_producers(queue, config, ctx);
    auto consumers = launch_validating_consumers(queue, config, ctx);

    ASSERT_TRUE(wait_for_completion(producers, config.timeout_seconds));

    queue.close();

    ASSERT_TRUE(wait_for_completion(consumers, config.timeout_seconds));
}

void expect_mpmc_data_integrity(const data_validator& validator,
                                size_t producer_count,
                                size_t items_per_producer) {
    size_t expected_total = producer_count * items_per_producer;
    EXPECT_EQ(validator.total_produced(), expected_total);
    EXPECT_EQ(validator.total_consumed(), expected_total);
    EXPECT_TRUE(validator.validate_no_loss());
}

TYPED_TEST(queue_stress, bounded_queue_fullness) {
    stress_test_config config = bounded_fullness_config();
    data_validator validator(config.producer_count, config.items_per_producer);

    run_mpmc_test(*this->queue_, config, validator);

    expect_mpmc_data_integrity(validator, config.producer_count, config.items_per_producer);
}

TYPED_TEST(queue_stress, push_pop_data_integrity) {
    stress_test_config config = push_pop_config();
    data_validator validator(config.producer_count, config.items_per_producer);

    run_mpmc_test(*this->queue_, config, validator);

    expect_mpmc_data_integrity(validator, config.producer_count, config.items_per_producer);
}

TYPED_TEST(queue_stress, dequeue_stress) {
    stress_test_config config = dequeue_stress_config();
    size_t item_count = std::min(config.items_per_producer, QUEUE_CAPACITY);

    for (size_t i = 0; i < item_count; ++i) {
        while (!this->queue_->enqueue(element_type{0, i})) {
            std::this_thread::yield();
        }
    }
    this->queue_->close();

    std::atomic<size_t> consumed_count{0};
    auto consumers = launch_counting_consumers(*this->queue_, config, consumed_count);

    ASSERT_TRUE(wait_for_completion(consumers, config.timeout_seconds));

    EXPECT_EQ(consumed_count.load(), item_count);
    EXPECT_TRUE(this->queue_->empty());
}

TYPED_TEST(queue_stress, spsc_stress) {
    stress_test_config config = spsc_config();
    data_validator validator(config.producer_count, config.items_per_producer);

    run_mpmc_test(*this->queue_, config, validator);

    expect_mpmc_data_integrity(validator, config.producer_count, config.items_per_producer);
}
