#include <gtest/gtest.h>

class Base {
   public:
    explicit Base(uint32_t num = 0) noexcept { num_ = new uint32_t(num); }
    ~Base() { Destory(); }
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
    uint32_t GetNum() { return *num_; }
    bool IsCopyCstr() const { return isCopyCstr_; }
    bool IsMoveCstr() const { return isMoveCstr_; }
    void Destory() {
        if (num_ != nullptr) {
            delete num_;
            num_ = nullptr;
        }
    }

   private:
    uint32_t* num_{nullptr};
    bool isCopyCstr_{false};
    bool isMoveCstr_{false};
};

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

TEST(SharedPtrUt, WeakPtrBasicUseCount) {
    std::shared_ptr<Base> sp = std::make_shared<Base>(10);
    std::weak_ptr<Base> wp = sp;

    // shared_ptr存在时weak_ptr的use_count
    EXPECT_EQ(wp.use_count(), 1);
    EXPECT_FALSE(wp.expired());

    // 增加shared_ptr引用
    std::shared_ptr<Base> sp2 = sp;
    EXPECT_EQ(wp.use_count(), 2);

    // 释放一个shared_ptr
    sp2.reset();
    EXPECT_EQ(wp.use_count(), 1);

    // 全部释放shared_ptr
    sp.reset();
    EXPECT_EQ(wp.use_count(), 0);
    EXPECT_TRUE(wp.expired());
}

TEST(SharedPtrUt, WeakPtrMultipleObservers) {
    auto sp = std::make_shared<Base>(20);
    std::weak_ptr<Base> wp1 = sp;
    std::weak_ptr<Base> wp2 = sp;

    EXPECT_EQ(wp1.use_count(), 1);
    EXPECT_EQ(wp2.use_count(), 1);

    std::shared_ptr<Base> sp2 = sp;
    EXPECT_EQ(wp1.use_count(), 2);
    EXPECT_EQ(wp2.use_count(), 2);

    sp.reset();
    EXPECT_EQ(wp1.use_count(), 1);
    EXPECT_EQ(wp2.use_count(), 1);
}

TEST(SharedPtrUt, WeakPtrLockBehavior) {
    std::weak_ptr<Base> wp;

    // 测试空的weak_ptr
    EXPECT_TRUE(wp.expired());
    EXPECT_EQ(wp.use_count(), 0);

    {
        auto sp = std::make_shared<Base>(30);
        wp = sp;
        EXPECT_EQ(wp.use_count(), 1);

        // 通过lock获取shared_ptr
        auto locked_sp = wp.lock();
        EXPECT_EQ(wp.use_count(), 2);
        EXPECT_TRUE(locked_sp != nullptr);
    }

    // 离开作用域后shared_ptr被释放
    EXPECT_EQ(wp.use_count(), 0);
    EXPECT_TRUE(wp.expired());
    EXPECT_TRUE(wp.lock() == nullptr);
}

TEST(SharedPtrUt, CyclicReference) {
    struct Node {
        std::shared_ptr<Node> next;
        std::weak_ptr<Node> prev;  // 使用weak_ptr打破循环引用
    };

    auto node1 = std::make_shared<Node>();
    auto node2 = std::make_shared<Node>();

    node1->next = node2;
    node2->prev = node1;

    // 验证use_count
    EXPECT_EQ(node1.use_count(), 1);
    EXPECT_EQ(node2.use_count(), 2);  // node2被node1->next和node2自己持有

    // prev使用weak_ptr不应增加引用计数
    EXPECT_EQ(node2->prev.use_count(), 1);

    node1.reset();
    node2.reset();
    // 如果weak_ptr使用正确，此处应能正确释放内存
}
