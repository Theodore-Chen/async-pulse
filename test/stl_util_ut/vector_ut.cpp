#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(VectorUt, Size0) {
    std::vector<uint32_t> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 0);
}

TEST(VectorUt, IteratorInvalidation) {
    std::vector<int> v = {1, 2, 3};
    auto it = v.begin();

    // 插入导致迭代器失效
    v.insert(v.begin(), 0);
    EXPECT_NE(it, v.begin());

    // 扩容导致迭代器失效
    it = v.begin();
    v.reserve(100);
    EXPECT_NE(it, v.begin());
}

TEST(VectorUt, InsertErase) {
    std::vector<std::string> v = {"a", "b", "c"};

    // 插入元素
    v.insert(v.begin() + 1, "x");
    EXPECT_EQ(v.size(), 4);
    EXPECT_EQ(v[1], "x");

    // 删除元素
    v.erase(v.begin());
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "x");
}

TEST(VectorUt, MoveSemantics) {
    std::vector<std::string> v1 = {"a", "b", "c"};
    std::vector<std::string> v2 = std::move(v1);

    EXPECT_TRUE(v1.empty());
    EXPECT_EQ(v2.size(), 3);
    EXPECT_EQ(v2[0], "a");
}

TEST(VectorUt, CustomType) {
    struct Point {
        int x, y;
        Point(int x, int y) : x(x), y(y) {}
    };

    std::vector<Point> v;
    v.emplace_back(1, 2);
    v.emplace_back(3, 4);

    EXPECT_EQ(v.size(), 2);
    EXPECT_EQ(v[0].x, 1);
    EXPECT_EQ(v[1].y, 4);
}

TEST(VectorUt, CapacityGrowth) {
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

TEST(VectorUt, ShrinkToFit) {
    // 测试shrink_to_fit的缩容效果
    std::vector<int> v;
    v.reserve(100);
    for (int i = 0; i < 9; ++i) {
        v.push_back(i);
    }
    EXPECT_GE(v.capacity(), 100);

    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), 9);

    v.clear();
    v.shrink_to_fit();
    EXPECT_EQ(v.capacity(), 0);
}

TEST(VectorUt, SwapTrick) {
    // 测试swap缩容技巧
    std::vector<std::string> v(100);
    v.resize(10);

    const auto old_capacity = v.capacity();
    std::vector<std::string>(v).swap(v);

    EXPECT_EQ(v.size(), 10);
    EXPECT_LT(v.capacity(), old_capacity);
    EXPECT_EQ(v.capacity(), v.size());
}

TEST(VectorUt, NoAutoShrink) {
    // 验证删除操作不会自动缩容
    std::vector<double> v;
    v.reserve(100);
    const auto initial_capacity = v.capacity();

    // 填充并删除元素
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    v.erase(v.begin(), v.end() - 10);

    EXPECT_EQ(v.size(), 10);
    EXPECT_EQ(v.capacity(), initial_capacity);  // capacity应保持不变

    // 再次填充并部分删除
    for (int i = 0; i < 50; ++i) {
        v.push_back(i);
    }
    v.resize(20);

    EXPECT_EQ(v.size(), 20);
    EXPECT_GE(v.capacity(), initial_capacity);  // capacity不应减少
}

TEST(VectorUt, PartialShrink) {
    // 测试部分缩容
    std::vector<int> v = {1, 2, 3, 4, 5};
    v.reserve(20);
    const auto old_capacity = v.capacity();

    v.resize(3);
    v.shrink_to_fit();

    EXPECT_EQ(v.size(), 3);
    EXPECT_LT(v.capacity(), old_capacity);
    EXPECT_EQ(v.capacity(), v.size());
}
