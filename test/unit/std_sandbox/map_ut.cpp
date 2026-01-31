#include <gtest/gtest.h>
#include <map>
#include <string>
#include <utility>

TEST(MapUt, BasicOperations) {
    std::map<int, std::string> m;

    // 插入元素
    m[1] = "one";
    m.insert({2, "two"});
    m.emplace(3, "three");

    EXPECT_EQ(m.size(), 3);
    EXPECT_EQ(m[1], "one");
    EXPECT_EQ(m.at(2), "two");

    // 查找元素
    auto it = m.find(3);
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->second, "three");

    // 删除元素
    m.erase(2);
    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.find(2), m.end());
}

TEST(MapUt, IteratorStability) {
    std::map<int, std::string> m = {{1, "one"}, {2, "two"}, {3, "three"}};

    // 迭代器在插入/删除时保持有效(除非删除当前元素)
    auto it = m.find(2);
    m[4] = "four";
    m.erase(1);

    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, "two");
}

TEST(MapUt, CustomComparator) {
    struct CaseInsensitiveCompare {
        bool operator()(const std::string& a, const std::string& b) const {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                                [](char c1, char c2) { return tolower(c1) < tolower(c2); });
        }
    };

    std::map<std::string, int, CaseInsensitiveCompare> m;
    m["Apple"] = 1;
    m["banana"] = 2;

    EXPECT_EQ(m.find("APPLE")->second, 1);
    EXPECT_EQ(m.find("BANANA")->second, 2);
    EXPECT_EQ(m.size(), 2);
}

TEST(MapUt, MoveSemantics) {
    std::map<int, std::string> m1 = {{1, "one"}, {2, "two"}};
    std::map<int, std::string> m2 = std::move(m1);

    EXPECT_TRUE(m1.empty());
    EXPECT_EQ(m2.size(), 2);
    EXPECT_EQ(m2[1], "one");
}

TEST(MapUt, BoundsChecking) {
    std::map<int, std::string> m = {{10, "ten"}, {20, "twenty"}, {30, "thirty"}};

    // 边界测试
    auto lb = m.lower_bound(15);
    EXPECT_EQ(lb->first, 20);

    auto ub = m.upper_bound(25);
    EXPECT_EQ(ub->first, 30);

    auto range = m.equal_range(20);
    EXPECT_EQ(range.first->first, 20);
    EXPECT_EQ(range.second->first, 30);
}

TEST(MapUt, PerformanceHint) {
    std::map<int, std::string> m;

    // 使用hint提高插入性能
    auto hint = m.end();
    for (int i = 0; i < 100; ++i) {
        // emplace_hint优化原理：
        // 1. hint迭代器提示插入位置，当提示正确时时间复杂度从O(logn)降为O(1)
        // 2. 对于有序插入场景（如递增序列），上一次插入位置即为最佳提示位置
        // 3. 每次插入返回新元素的迭代器，作为下一次插入的hint
        hint = m.emplace_hint(hint, i, std::to_string(i));
    }
    // 优化效果：
    // - 总时间复杂度从O(n logn)降为O(n)
    // - 避免每次从头开始查找插入位置
    // - 利用红黑树的有序特性最小化节点比较次数

    EXPECT_EQ(m.size(), 100);
    EXPECT_EQ(m[50], "50");
}

TEST(MapUt, LvalueRvalueCopy) {
    std::map<int, std::string> m1 = {{1, "one"}, {2, "two"}};

    // 左值拷贝构造
    std::map<int, std::string> m2(m1);
    EXPECT_EQ(m2.size(), 2);
    EXPECT_EQ(m2[1], "one");

    // 右值移动构造
    std::map<int, std::string> m3(std::move(m1));
    EXPECT_TRUE(m1.empty());
    EXPECT_EQ(m3.size(), 2);
    EXPECT_EQ(m3[2], "two");
}

// 自定义分配器跟踪内存分配（修复引用计数问题）
template <typename T>
struct TrackingAllocator {
    using value_type = T;
    size_t& alloc_count;  // 引用方式持有外部计数器
    size_t& dealloc_count;

    // 必须通过引用初始化
    TrackingAllocator(size_t& a, size_t& d) : alloc_count(a), dealloc_count(d) {}

    // 拷贝构造函数必须保持同一组计数器
    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>& other)
        : alloc_count(other.alloc_count), dealloc_count(other.dealloc_count) {}

    T* allocate(size_t n) {
        alloc_count++;
        // ::operator new 表示调用全局命名空间的原始内存分配函数
        // 与常规new表达式不同，它只分配内存不调用构造函数
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) {
        dealloc_count++;
        // ::operator delete 对应释放由::operator new分配的内存
        // 与常规delete不同，它只释放内存不调用析构函数
        ::operator delete(p);
    }
};

// 自定义分配器测试（简化版）
TEST(MapUt, CustomAllocator) {
    size_t allocs = 0;
    size_t deallocs = 0;

    {
        using AllocType = TrackingAllocator<std::pair<const int, std::string>>;
        AllocType alloc(allocs, deallocs);
        std::map<int, std::string, std::less<int>, AllocType> m(alloc);

        // 验证初始内存状态
        EXPECT_EQ(allocs, 0);
        EXPECT_EQ(deallocs, 0);

        // 插入多个元素验证分配器计数
        m[1] = "one";
        m.emplace(2, "two");
        m.insert({3, "three"});

        EXPECT_EQ(m.size(), 3);
        EXPECT_EQ(allocs, 3);  // 精确匹配分配次数

        // 删除元素验证释放计数
        m.erase(2);
        EXPECT_EQ(m.size(), 2);
        EXPECT_GT(deallocs, 0);
    }

    // 验证完全释放
    EXPECT_EQ(allocs, deallocs);
    EXPECT_EQ(allocs, 3);
}
