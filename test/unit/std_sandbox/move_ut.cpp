#include <gtest/gtest.h>

class Base {
   public:
    explicit Base(uint32_t num = 0) noexcept : num_(num) {}
    Base(const Base& other) {
        num_ = other.num_;
        isCopyCstr_ = true;
    }
    Base(Base&& other) noexcept {
        num_ = other.num_;
        isMoveCstr_ = true;
    }
    Base& operator=(const Base& other) {
        num_ = other.num_;
        return *this;
    }
    Base& operator=(Base&& other) {
        num_ = other.num_;
        return *this;
    }
    ~Base() = default;
    bool IsMoveCstr() {
        return isMoveCstr_;
    }
    bool IsCopyCstr() {
        return isCopyCstr_;
    }
    uint32_t GetNum() {
        return num_;
    }

   private:
    uint32_t num_{0};
    bool isMoveCstr_{false};
    bool isCopyCstr_{false};
};

TEST(MoveUt, ResizeVectorWithException) {
    std::vector<Base> base;
    size_t initSize = 10;
    base.reserve(initSize);
    for (size_t i = 0; i < initSize; i++) {
        base.emplace_back(i);
    }
    for (auto& b : base) {
        EXPECT_FALSE(b.IsMoveCstr());
        EXPECT_FALSE(b.IsCopyCstr());
    }
    EXPECT_EQ(base.size(), initSize);
    EXPECT_GT(base.max_size(), initSize);
    EXPECT_GE(base.capacity(), initSize);

    base.reserve(initSize * 2);
    for (auto& b : base) {
        EXPECT_TRUE(b.IsMoveCstr());
        EXPECT_FALSE(b.IsCopyCstr());
    }
}

// 不可移动类型测试
class NonMovableObj {
   public:
    static int copy_count;
    int value{0};

    explicit NonMovableObj(int v) : value(v) {}
    NonMovableObj(const NonMovableObj& other) {
        copy_count++;
        value = other.value;
    }
    NonMovableObj(NonMovableObj&&) = delete;
};
int NonMovableObj::copy_count = 0;

TEST(MoveUt, NonMovableElement) {
    NonMovableObj::copy_count = 0;
    std::vector<NonMovableObj> vec;
    vec.reserve(1);
    vec.emplace_back(1);
    vec.emplace_back(2); // 触发扩容

    EXPECT_EQ(NonMovableObj::copy_count, 1); // 原元素拷贝
    EXPECT_EQ(vec[0].value, 1);
    EXPECT_EQ(vec[1].value, 2);
}

// noexcept移动类型测试
class NoexceptMovableObj {
   public:
    static int move_count;
    int value{0};

    explicit NoexceptMovableObj(int v) : value(v) {}
    NoexceptMovableObj(NoexceptMovableObj&& other) noexcept {
        move_count++;
        value = other.value;
    }
    NoexceptMovableObj(const NoexceptMovableObj&) = delete;
};
int NoexceptMovableObj::move_count = 0;

TEST(MoveUt, NoexceptMoveOptimization) {
    NoexceptMovableObj::move_count = 0;
    std::vector<NoexceptMovableObj> vec;
    vec.reserve(1);
    vec.emplace_back(1);
    vec.emplace_back(2); // 触发扩容

    EXPECT_EQ(NoexceptMovableObj::move_count, 1);
    EXPECT_EQ(vec[0].value, 1);
    EXPECT_EQ(vec[1].value, 2);
}

// 异常安全测试类型
class ThrowOnMoveObj {
   public:
    static int instance_count;
    int value{0};

    explicit ThrowOnMoveObj(int v) : value(v) {
        if (instance_count >= 2) {
            throw std::runtime_error("default construct failed"); // 只有构造时报异常会回退
        }
        instance_count++;
    }
    ThrowOnMoveObj(const ThrowOnMoveObj& other) : value(other.value) {
        instance_count++;
    }
    ThrowOnMoveObj(ThrowOnMoveObj&& other) noexcept : value(other.value) {
        instance_count++;
    }
    ~ThrowOnMoveObj() {
        instance_count--;
    }
};
int ThrowOnMoveObj::instance_count = 0;

TEST(MoveUt, ExceptionSafety) {
    ThrowOnMoveObj::instance_count = 0;
    std::vector<ThrowOnMoveObj> vec;
    vec.emplace_back(1);
    vec.emplace_back(2);
    EXPECT_EQ(vec.capacity(), 2);

    EXPECT_THROW(vec.emplace_back(3), std::runtime_error);

    EXPECT_EQ(vec.size(), 2) << "Vector size should rollback after exception";
    EXPECT_EQ(vec.capacity(), 2) << "Capacity should remain unchanged";
    EXPECT_EQ(ThrowOnMoveObj::instance_count, 2) << "Instance count mismatch";

    // 验证元素有效性
    EXPECT_EQ(vec[0].value, 1);
    EXPECT_EQ(vec[1].value, 2);
}
