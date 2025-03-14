#include <gtest/gtest.h>

class Base {
   public:
    explicit Base(uint32_t num = 0) noexcept { num_ = new uint32_t(num); }
    ~Base() { Destory(); }
    Base(const Base& other) {
        num_ = other.num_;
        isCopyCstr = true;
    }
    Base(Base&& other) noexcept {
        num_ = other.num_;
        isMoveCstr = true;
    }
    Base& operator=(const Base& other) {
        num_ = other.num_;
        isCopyAssign = true;
        return *this;
    }
    Base& operator=(Base&& other) {
        num_ = other.num_;
        isMoveAssign = true;
        return *this;
    }
    uint32_t GetNum() { return *num_; }
    void Destory() {
        if (num_ != nullptr) {
            delete num_;
            num_ = nullptr;
        }
    }

   public:
    static bool isMoveCstr;
    static bool isCopyCstr;
    static bool isMoveAssign;
    static bool isCopyAssign;

   private:
    uint32_t* num_{nullptr};
};

bool Base::isMoveCstr = false;
bool Base::isCopyCstr = false;
bool Base::isMoveAssign = false;
bool Base::isCopyAssign = false;

TEST(SharedPtrUt, Size) {
    std::shared_ptr<Base> base = std::make_shared<Base>(uint32_t(5));
    EXPECT_TRUE(static_cast<bool>(base));
    EXPECT_EQ(sizeof(base), sizeof(nullptr) * 2);
}

TEST(SharedPtrUt, SizeWithDeleter) {
    std::shared_ptr<Base> baseDel(new Base(uint32_t(10)), [](Base* b) {
        if (b != nullptr) {
            b->Destory();
            delete b;
        }
    });
    EXPECT_TRUE(static_cast<bool>(baseDel));
    EXPECT_EQ(sizeof(baseDel), sizeof(void*) * 2);
}

TEST(SharedPtrUt, UseCount) {
    std::shared_ptr<Base> base = std::make_shared<Base>(uint32_t(5));
    EXPECT_EQ(base.use_count(), 1);
    
    std::shared_ptr<Base> base2 = base;
    EXPECT_EQ(base.use_count(), 2);
    EXPECT_EQ(base2.use_count(), 2);

    base2.reset();
    EXPECT_EQ(base.use_count(), 1);

    base.reset();
    EXPECT_EQ(base.use_count(), 0);
}
