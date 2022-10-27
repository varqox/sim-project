#include <gtest/gtest.h>
#include <simlib/constructor_traits.hh>

struct A {};
struct B {};

struct Tester {
    // NOLINTNEXTLINE(google-explicit-constructor)
    Tester(A /*unused*/) {}
    Tester(A /*unused*/, A /*unused*/) {}
    explicit Tester(B /*unused*/) {}
    explicit Tester(B /*unused*/, B /*unused*/) {}
};

struct TesterDefaultImplicit {
    TesterDefaultImplicit();
};

struct TesterDefaultExplicit {
    explicit TesterDefaultExplicit();
};

// NOLINTNEXTLINE
TEST(constructor_traits, is_implicitly_constructible_v) {
    static_assert(is_implicitly_constructible_v<Tester, A>);
    static_assert(is_implicitly_constructible_v<Tester, A, A>);
    static_assert(!is_implicitly_constructible_v<Tester, B>);
    static_assert(!is_implicitly_constructible_v<Tester, B, B>);
    // Invalid argument combinations
    static_assert(!is_implicitly_constructible_v<Tester>);
    static_assert(!is_implicitly_constructible_v<Tester, int>);
    static_assert(!is_implicitly_constructible_v<Tester, int, int>);

    static_assert(is_implicitly_constructible_v<TesterDefaultImplicit>);
    static_assert(!is_implicitly_constructible_v<TesterDefaultExplicit>);
}

// NOLINTNEXTLINE
TEST(constructor_traits, is_constructor_ok) {
    static_assert(!is_constructor_ok<A, A>);
    static_assert(!is_constructor_ok<A, A&>);
    static_assert(!is_constructor_ok<A, const A>);
    static_assert(!is_constructor_ok<A, const A&>);

    static_assert(is_constructor_ok<A>);

    static_assert(is_constructor_ok<A, B>);
    static_assert(is_constructor_ok<A, B&>);
    static_assert(is_constructor_ok<A, const B>);
    static_assert(is_constructor_ok<A, const B&>);

    static_assert(is_constructor_ok<A, A, A>);
    static_assert(is_constructor_ok<A, A, A&>);
    static_assert(is_constructor_ok<A, A, const A>);
    static_assert(is_constructor_ok<A, A, const A&>);

    static_assert(is_constructor_ok<A, A&, A>);
    static_assert(is_constructor_ok<A, A&, A&>);
    static_assert(is_constructor_ok<A, A&, const A>);
    static_assert(is_constructor_ok<A, A&, const A&>);

    static_assert(is_constructor_ok<A, const A, A>);
    static_assert(is_constructor_ok<A, const A, A&>);
    static_assert(is_constructor_ok<A, const A, const A>);
    static_assert(is_constructor_ok<A, const A, const A&>);

    static_assert(is_constructor_ok<A, const A&, A>);
    static_assert(is_constructor_ok<A, const A&, A&>);
    static_assert(is_constructor_ok<A, const A&, const A>);
    static_assert(is_constructor_ok<A, const A&, const A&>);
}

struct TesterInheriting {};

// NOLINTNEXTLINE
TEST(constructor_traits, is_inheriting_explicit_constructor_ok) {
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, TesterDefaultImplicit>);
    static_assert(is_inheriting_explicit_constructor_ok<TesterInheriting, TesterDefaultExplicit>);
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, Tester>);
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, A>);
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, A, A>);
    static_assert(is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, B>);
    static_assert(is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, B, B>);
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, A, B>);
    static_assert(!is_inheriting_explicit_constructor_ok<TesterInheriting, Tester, B, A>);
}

// NOLINTNEXTLINE
TEST(constructor_traits, is_inheriting_implicit_constructor_ok) {
    static_assert(is_inheriting_implicit_constructor_ok<TesterInheriting, TesterDefaultImplicit>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, TesterDefaultExplicit>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, Tester>);
    static_assert(is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, A>);
    static_assert(is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, A, A>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, B>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, B, B>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, A, B>);
    static_assert(!is_inheriting_implicit_constructor_ok<TesterInheriting, Tester, B, A>);
}
