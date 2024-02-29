#include <cstdio>
#include <gtest/gtest.h>
#include <simlib/macros/wont_throw.hh>

// NOLINTNEXTLINE
TEST(macros_DeathTest, WONT_THROW_MACRO_fail) {
    EXPECT_EXIT(
        {
            std::vector<int> abc;
            (void)fprintf(stderr, "BUG\n");
            (void)WONT_THROW(abc.at(42));
        },
        ::testing::KilledBySignal(SIGABRT),
        "^BUG\nBUG: this was not expected to throw: abc.at\\(42\\) \\(at "
    );
}

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_lvalue_reference) {
    {
        int a = 42;
        WONT_THROW(++a) = 42;
        EXPECT_EQ(a, 42);
    }
    {
        int a = 42;
        int& x = WONT_THROW(++a);
        ++a;
        EXPECT_EQ(x, 44);
    }
}

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_rvalue_reference) {
    auto ptr = std::make_unique<int>(162);
    std::unique_ptr<int> ptr2 = WONT_THROW(std::move(ptr));
    EXPECT_EQ((bool)ptr, false);
    EXPECT_EQ((bool)ptr2, true);
}

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_xvalue) {
    struct A {
        std::string& str;
        A(const A&) = delete;
        A(A&&) = delete;
        A& operator=(const A&) = delete;
        A& operator=(A&&) = delete;

        explicit A(std::string& s) : str{s} { str += "+A"; }

        ~A() { str += "-A"; }
    };

    struct B {
        A a;
        std::string& str;
        B(const B&) = delete;
        B(B&&) = delete;
        B& operator=(const B&) = delete;
        B& operator=(B&&) = delete;

        explicit B(std::string& str) : a{str}, str{str} { str += "+B"; }

        ~B() { str += "-B"; }

        A&& get() && noexcept { return std::move(a); }
    };

    struct C {
        std::string& str;
        C(const C&) = delete;
        C(C&&) = delete;
        C& operator=(const C&) = delete;
        C& operator=(C&&) = delete;

        C(std::string& s, const A& /*unused*/) : str{s} { str += "+C"; }

        ~C() { str += "-C"; }
    };

    std::string str;
    {
        C val(str, B(str).get());
    }
    std::string wont_throw_str;
    {
        C val(wont_throw_str, WONT_THROW(B(wont_throw_str).get()));
    }
    EXPECT_EQ(str, wont_throw_str);
}

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_prvalue) {
    std::unique_ptr<int> ptr = WONT_THROW(std::make_unique<int>(162));
    EXPECT_EQ((bool)ptr, true);
}

template <class T, class = decltype(T(WONT_THROW(T(42))))>
constexpr std::true_type double_construct_with_WONT_THROW_impl(int);

template <class>
constexpr std::false_type double_construct_with_WONT_THROW_impl(...);

template <class T>
constexpr bool double_construct_with_WONT_THROW =
    decltype(double_construct_with_WONT_THROW_impl<T>(0))::value;

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_prvalue_copy_elision) {
    struct X {
        explicit X(int /*unused*/) {}

        X(const X&) = delete;
        X(X&&) = delete;
        X& operator=(const X&) = delete;
        X& operator=(X&&) = delete;
        ~X() = default;
    };

    struct Y {
        explicit Y(int /*unused*/) {}

        Y(const Y&) = delete;
        Y(Y&&) = default;
        Y& operator=(const Y&) = delete;
        Y& operator=(Y&&) = default;
        ~Y() = default;
    };

    // Sadly WONT_THROW() does not work with prvalues
    static_assert(not double_construct_with_WONT_THROW<X>);
    // But works with x-values
    static_assert(double_construct_with_WONT_THROW<Y>);
}

// NOLINTNEXTLINE
TEST(macros, WONT_THROW_MACRO_rvalue) {
    int a = 4;
    int b = 7;
    int c = WONT_THROW(a + b);
    EXPECT_EQ(a, 4);
    EXPECT_EQ(b, 7);
    EXPECT_EQ(c, 11);
}

// NOLINTNEXTLINE
TEST(macros_DeathTest, WONT_THROW_MACRO_throw_in_the_same_statement_after_WONT_THROW_MACRO) {
    try {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        auto factory = [] { return [] { throw 42; }; };
        WONT_THROW(factory())();
        FAIL() << "should have thrown";
    } catch (int x) {
        EXPECT_EQ(x, 42);
    }
}
