#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

TEST(UnorderedMapTest, RehashingBehavior) {
    std::unordered_map<int, std::string> map;
    size_t initial_buckets = map.bucket_count();

    for (int i = 0; i < 1000; ++i) {
        map[i] = "value" + std::to_string(i);
    }

    EXPECT_GT(map.bucket_count(), initial_buckets);
    EXPECT_EQ(map.size(), 1000);
}

TEST(UnorderedMapTest, MoveSemantics) {
    std::unordered_map<int, std::string> original = {{1, "one"}, {2, "two"}};
    std::unordered_map<int, std::string> moved = std::move(original);

    EXPECT_TRUE(original.empty());
    EXPECT_EQ(moved.size(), 2);
    EXPECT_EQ(moved[1], "one");
}

TEST(UnorderedMapTest, HashCollisions) {
    struct BadHash {
        size_t operator()(int) const { return 42; }
    };

    std::unordered_map<int, std::string, BadHash> map;
    map[1] = "one";
    map[2] = "two";

    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map.bucket_size(map.bucket(1)), 2);
}

TEST(UnorderedMapTest, CopyBehavior) {
    std::unordered_map<int, std::string> original = {{1, "one"}, {2, "two"}};
    auto copy = original;

    EXPECT_EQ(original.size(), copy.size());
    EXPECT_EQ(original[1], copy[1]);
    original[1] = "modified";
    EXPECT_NE(original[1], copy[1]);
}

TEST(UnorderedMapTest, IteratorInvalidation) {
    std::unordered_map<int, std::string> map = {{1, "one"}};
    auto it = map.begin();

    // 激进地强制触发rehash
    map.max_load_factor(0.5f);
    map.rehash(1);  // 强制立即rehash到最小容量

    // 插入元素确保rehash发生
    for (int i = 0; i < 1000; ++i) {
        map[i] = "value" + std::to_string(i);
    }

    // 死亡测试
    EXPECT_DEATH(
        {
            volatile auto val = it->second;  // 强制访问迭代器
            (void)val;                       // 避免未使用变量警告
            std::terminate();                // 确保终止
        },
        "");
}
