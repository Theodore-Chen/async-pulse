#include <gtest/gtest.h>

#include "queue/lock_bounded_queue.h"
#include "queue/lock_free_bounded_queue.h"
#include "queue_factory.h"

template <typename T>
class bounded_queue_ut : public ::testing::Test {
   protected:
    using queue_type = T;
    using element_type = typename queue_type::value_type;

   protected:
    void SetUp() override {
        using factory = queue_factory<queue_type, 128>;
        queue_ = factory::create();
        capacity_ = factory::capacity_;
    }
    std::unique_ptr<queue_type> queue_;
    size_t capacity_{0};
};

using bounded_queue_impls = ::testing::Types<lock_bounded_queue<uint32_t>, lock_free_bounded_queue<uint32_t>>;

TYPED_TEST_SUITE(bounded_queue_ut, bounded_queue_impls);

TYPED_TEST(bounded_queue_ut, queue_full) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    for (size_t i = 0; i < this->capacity_; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }

    element_type dummy;
    EXPECT_FALSE(queue.try_enqueue_with([&dummy](element_type& dest) { dest = 999; }));

    for (size_t i = 0; i < this->capacity_; ++i) {
        element_type value;
        EXPECT_TRUE(queue.dequeue(value));
        EXPECT_EQ(value, static_cast<element_type>(i));
    }

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TYPED_TEST(bounded_queue_ut, is_full) {
    using queue_type = typename TestFixture::queue_type;
    using element_type = typename TestFixture::element_type;

    queue_type& queue = *(this->queue_);

    EXPECT_FALSE(queue.is_full());

    for (size_t i = 0; i < this->capacity_; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<element_type>(i)));
    }

    EXPECT_TRUE(queue.is_full());

    element_type value;
    EXPECT_TRUE(queue.dequeue(value));
    EXPECT_FALSE(queue.is_full());
}

TYPED_TEST(bounded_queue_ut, capacity) {
    using queue_type = typename TestFixture::queue_type;

    queue_type& queue = *(this->queue_);

    EXPECT_EQ(queue.capacity(), this->capacity_);
}

TYPED_TEST(bounded_queue_ut, try_dequeue_empty) {
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
