#include <gtest/gtest.h>
#include <unordered_set>

TEST(VectorUt, Size0) {
    std::vector<uint32_t> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
}

TEST(VectorUt, Capacity) {
    std::vector<uint32_t> v;
    uint32_t cpt = 1;
    auto upper2power = [&cpt](uint32_t size) {
        if (size > cpt) {
            cpt *= 2;
        }
        return cpt;
    };

    for (uint32_t i = 1; i < 1026; i++) {
        v.push_back(i);
        EXPECT_EQ(v.size(), i);
        EXPECT_EQ(v.capacity(), upper2power(i));
    }
}
