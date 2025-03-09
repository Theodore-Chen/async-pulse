#include <gtest/gtest.h>
#include <gmock/gmock.h>

template <typename Derived>
class Base {
   public:
    void Interface() { static_cast<Derived*>(this)->Implementation(); }
};

class Derived : public Base<Derived> {
   public:
    virtual void Implementation() {}
};

template <typename Deriver>
class Wrapper {
   public:
    Wrapper(Deriver& derived) : derived_(derived) {}
    void Caller() { derived_.Implementation(); }

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
    Counter() { ++count; }
    ~Counter() { --count; }
    static int GetCount() { return count; }

   private:
    static int count;
};

template <typename Obj>
int Counter<Obj>::count = 0;

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
