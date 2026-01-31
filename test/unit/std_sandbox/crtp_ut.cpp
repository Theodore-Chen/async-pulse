#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <type_traits>

template <typename Derived>
class Base {
   public:
    void Interface() {
        static_cast<Derived*>(this)->Implementation();
    }
};

class Derived : public Base<Derived> {
   public:
    virtual void Implementation() {}
};

template <typename Deriver>
class Wrapper {
   public:
    Wrapper(Deriver& derived) : derived_(derived) {}
    void Caller() {
        derived_.Implementation();
    }

   private:
    Deriver& derived_;
};

TEST(CrtpUt, Base) {
    class MockDerived : public Derived {
       public:
        MOCK_METHOD(void, Implementation, (), (override));
    };
    MockDerived mockDerived;
    EXPECT_CALL(mockDerived, Implementation()).Times(1);
    mockDerived.Interface();
}

template <typename Obj>
class Counter {
   public:
    Counter() {
        ++count;
    }
    ~Counter() {
        --count;
    }
    static uint32_t GetCount() {
        return count;
    }

   private:
    static uint32_t count;
};

template <typename Obj>
uint32_t Counter<Obj>::count = 0;

class ObjLeft : public Counter<ObjLeft> {};
class ObjRight : public Counter<ObjRight> {};

TEST(CrtpUt, CounterCstr) {
    ObjLeft left;
    EXPECT_EQ(ObjLeft::GetCount(), 1);
    ObjLeft left2;
    EXPECT_EQ(ObjLeft::GetCount(), 2);

    ObjRight right;
    EXPECT_EQ(ObjRight::GetCount(), 1);
}

TEST(CrtpUt, CounterDstr) {
    ObjLeft* left = new ObjLeft();
    ObjLeft* left2 = new ObjLeft();
    ObjRight* right = new ObjRight();

    delete left;
    EXPECT_EQ(ObjLeft::GetCount(), 1);
    delete left2;
    EXPECT_EQ(ObjLeft::GetCount(), 0);
    delete right;
    EXPECT_EQ(ObjRight::GetCount(), 0);
}

// 静态多态测试
template <typename T>
struct Shape : T {
    void draw() const {
        T::draw_impl(*this);
    }
};

struct Circle {
    static void draw_impl(const Shape<Circle>&) {}
};
struct Square {
    static void draw_impl(const Shape<Square>&) {}
};

TEST(CrtpUt, StaticPolymorphism) {
    Shape<Circle> c;
    Shape<Square> s;
    static_assert(sizeof(c) == sizeof(Circle), "No vtable overhead");
    static_assert(sizeof(s) == sizeof(Square), "No vtable overhead");
}

// 运算符链式调用测试
template <typename Derived>
class Chainable {
   public:
    Derived& operator++() {
        ++static_cast<Derived*>(this)->value;
        return *static_cast<Derived*>(this);
    }

    Derived operator++(int) {
        Derived tmp(*static_cast<Derived*>(this));
        ++(*this);
        return tmp;
    }
};

class ChainableCounter : public Chainable<ChainableCounter> {
   public:
    int value = 0;
};

TEST(CrtpUt, ChainingOperators) {
    ChainableCounter c;
    EXPECT_EQ((c++).value, 0);
    EXPECT_EQ(c.value, 1);
    EXPECT_EQ((++c).value, 2);
}

// 类型特征检查测试
template <typename T>
struct Cloneable {
    T clone() const {
        return static_cast<const T*>(this)->clone_impl();
    }
};

struct Widget : Cloneable<Widget> {
    int id = 0;
    Widget clone_impl() const {
        Widget w;
        w.id = id + 1;
        return w;
    }
};

TEST(CrtpUt, TypeTraits) {
    Widget w{};
    auto w2 = w.clone();
    static_assert(std::is_same_v<decltype(w2), Widget>, "Clone should return derived type");
    EXPECT_EQ(w2.id, 1);
}

// 异常传播测试
template <typename Derived>
class ExceptionBase {
   public:
    void execute() {
        // 使用更安全的SFINAE检测方式
        if constexpr (has_throw_if_invalid<Derived>::value) {
            static_cast<Derived*>(this)->throw_if_invalid();
        }
        static_cast<Derived*>(this)->do_execute();
    }

   private:
    // 改进的类型特征检测模板
    template <typename U, typename = void>
    struct has_throw_if_invalid : std::false_type {};

    template <typename U>
    struct has_throw_if_invalid<U, std::void_t<decltype(std::declval<U&>().throw_if_invalid())>> : std::true_type {};
};

struct ValidOp : ExceptionBase<ValidOp> {
    void do_execute() {}
};

struct InvalidOp : ExceptionBase<InvalidOp> {
    void throw_if_invalid() {
        throw std::runtime_error("invalid");
    }
    void do_execute() {}
};

TEST(CrtpUt, ExceptionPropagation) {
    ValidOp valid;
    EXPECT_NO_THROW(valid.execute());

    InvalidOp invalid;
    EXPECT_THROW(invalid.execute(), std::runtime_error);
}
