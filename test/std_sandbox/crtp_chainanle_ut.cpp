#include <gtest/gtest.h>

template <typename Derived>
class Chainable {
   public:
    Derived& Self() { return static_cast<Derived&>(*this); }

    Derived& Method1() { return Self(); }

    Derived& Method2() { return Self(); }
};

class Widget : public Chainable<Widget> {
   public:
    Widget& Method1() {
        value_ += 10;
        return *this;
    }

    Widget& SetValue(uint32_t value) {
        value_ = value;
        return *this;
    }

    uint32_t GetValue() { return value_; }

   private:
    uint32_t value_{0};
};

TEST(CrtpChainableUt, ChainableWidget) {
    Widget w;
    EXPECT_EQ(w.Method1().GetValue(), 10);
    EXPECT_EQ(w.Method2().GetValue(), 10);
    EXPECT_EQ(w.Method1().Method2().GetValue(), 20);
    EXPECT_EQ(w.SetValue(42).GetValue(), 42);
}

class SqlQueryBuilder : public Chainable<SqlQueryBuilder> {
   public:
    SqlQueryBuilder& select(const std::string& columns) {
        query_ += "SELECT " + columns + " ";
        return *this;
    }

    SqlQueryBuilder& from(const std::string& table) {
        query_ += "FROM " + table + " ";
        return *this;
    }

    SqlQueryBuilder& where(const std::string& condition) {
        query_ += "WHERE " + condition + " ";
        return *this;
    }

    std::string query() const { return query_; }

   private:
    std::string query_;
};

TEST(CrtpChainableUt, ChainableSqlQueryBuilder) {
    //  构造链式调用
    SqlQueryBuilder query;
    EXPECT_EQ(query.select("name, age").from("users").where("age >= 18").query(),
              std::string("SELECT name, age FROM users WHERE age >= 18 "));
}