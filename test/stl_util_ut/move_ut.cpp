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
    bool IsMoveCstr() { return isMoveCstr_; }
    bool IsCopyCstr() { return isCopyCstr_; }
    uint32_t GetNum() { return num_; }

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
