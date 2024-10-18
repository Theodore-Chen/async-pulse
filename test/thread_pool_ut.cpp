#include <gtest/gtest.h>
#include "thread_pool.h"

struct UtTestData {
    uint32_t id = 0;
    uint32_t out_put = 0;
};

void UtTestFunc(UtTestData& data) {
    volatile uint32_t a = 0;
    for (size_t i = 0; i < 10000; i++) {
        a += 1;
    }
    data.out_put = data.id * 2;
};

TEST(ThreadPoolUt, Create) {
    ThreadPool<UtTestData> threadPool(UtTestFunc);
    EXPECT_EQ(threadPool.GetThreadNum(), threadPool.THREAD_NUM_DEFAULT);
}
