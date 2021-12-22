#include "simlib/ranges.hh"

#include <gtest/gtest.h>
#include <memory>

using std::array;
using std::is_same_v;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

// NOLINTNEXTLINE
TEST(ranges, reverse_view_empty) {
    vector<int> a;
    vector<int> b;
    for (int x : reverse_view(a)) {
        b.emplace_back(x);
    }

    EXPECT_EQ(b, a);
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_lvalue) {
    vector a = {1, 2, 3, 4, 5};
    vector<int> b;
    for (int x : reverse_view(a)) {
        b.emplace_back(x);
    }

    EXPECT_EQ(b, (vector{5, 4, 3, 2, 1}));

    vector<int> v{1};
    for (int x : reverse_view(v)) {
        EXPECT_NE(&v[0], &x);
    }
    for (auto& x : reverse_view(v)) {
        EXPECT_EQ(&v[0], &x);
    }
    for (auto const& x : reverse_view(v)) {
        EXPECT_EQ(&v[0], &x);
    }
    for (auto&& x : reverse_view(v)) {
        EXPECT_EQ(&v[0], &x);
    }
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_double_lvalue) {
    vector<int> a{1, 2, 3, 4, 5};
    vector<int> b;
    for (int x : reverse_view(reverse_view(a))) {
        b.emplace_back(x);
    }

    EXPECT_EQ(b, (vector{1, 2, 3, 4, 5}));

    vector<int> v{1};
    for (int x : reverse_view(reverse_view(v))) {
        EXPECT_NE(&v[0], &x);
    }
    for (auto& x : reverse_view(reverse_view(v))) {
        EXPECT_EQ(&v[0], &x);
    }
    for (auto const& x : reverse_view(reverse_view(v))) {
        EXPECT_EQ(&v[0], &x);
    }
    for (auto&& x : reverse_view(reverse_view(v))) {
        EXPECT_EQ(&v[0], &x);
    }
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_simple_xvalue) {
    vector<int> b;
    for (int x : reverse_view(vector{1, 2, 3, 4, 5})) {
        b.emplace_back(x);
    }

    EXPECT_EQ(b, (vector{5, 4, 3, 2, 1}));
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_double_xvalue) {
    vector<int> b;
    for (int x : reverse_view(reverse_view(vector{1, 2, 3, 4, 5}))) {
        b.emplace_back(x);
    }

    EXPECT_EQ(b, (vector{1, 2, 3, 4, 5}));
}

namespace {

struct inc_dead {
    int* p;
    inc_dead(const inc_dead&) = delete;
    inc_dead(inc_dead&&) = delete;
    inc_dead& operator=(const inc_dead&) = delete;
    inc_dead& operator=(inc_dead&&) = delete;
    ~inc_dead() { ++*p; }
};

} // namespace

// NOLINTNEXTLINE
TEST(ranges, reverse_view_on_xvalue_lifetime) {
    int dead = 0;
    for (auto& x [[maybe_unused]] : reverse_view(array{inc_dead{&dead}, inc_dead{&dead}})) {
        EXPECT_EQ(dead, 0);
    }
    EXPECT_EQ(dead, 2);
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_double_on_xvalue_lifetime) {
    int dead = 0;
    for (auto& x [[maybe_unused]] :
            reverse_view(reverse_view(array{inc_dead{&dead}, inc_dead{&dead}})))
    {
        EXPECT_EQ(dead, 0);
    }
    EXPECT_EQ(dead, 2);
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_c_array) {
    int arr[] = {1, 2, 3};
    vector<int> res;
    for (int x : reverse_view(arr)) {
        res.emplace_back(x);
    }
    EXPECT_EQ(res, (vector{3, 2, 1}));
}

// NOLINTNEXTLINE
TEST(ranges, reverse_view_and_sort) {
    vector v{4, 2, 5, 3, 1};
    auto rv = reverse_view(v);
    std::sort(rv.begin(), rv.end());
    EXPECT_EQ(v, (vector{5, 4, 3, 2, 1}));
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_lvalue_noref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto x : v) {
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
    for (auto [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_lvalue_ref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto& x : v) {
        static_assert(is_same_v<decltype(x), int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_lvalue_cref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto const& x : v) {
        static_assert(is_same_v<decltype(x), const int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto const& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_lvalue_uref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto&& x : v) {
        static_assert(is_same_v<decltype(x), int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto&& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_lvalue_noref) {
    const vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto x : v) {
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
    for (auto [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_lvalue_ref) {
    const vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto& x : v) {
        static_assert(is_same_v<decltype(x), const int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_lvalue_cref) {
    const vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto const& x : v) {
        static_assert(is_same_v<decltype(x), const int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto const& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_lvalue_uref) {
    const vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto&& x : v) {
        static_assert(is_same_v<decltype(x), const int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    for (auto&& [i, x] : enumerate_view(v)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_xvalue_noref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto x : std::move(v)) {
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
    v = {1};
    v0_addr = &v[0];
    for (auto [i, x] : enumerate_view(std::move(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_xvalue_ref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto& x : std::move(v)) {
        static_assert(is_same_v<decltype(x), int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    v = {1};
    v0_addr = &v[0];
    for (auto& [i, x] : enumerate_view(std::move(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_xvalue_cref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto const& x : std::move(v)) {
        static_assert(is_same_v<decltype(x), const int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    v = {1};
    v0_addr = &v[0];
    for (auto const& [i, x] : enumerate_view(std::move(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_xvalue_uref) {
    vector<int> v{1};
    auto v0_addr = &v[0];
    for (auto&& x : std::move(v)) {
        static_assert(is_same_v<decltype(x), int&>);
        EXPECT_EQ(&x, v0_addr);
    }
    v = {1};
    v0_addr = &v[0];
    for (auto&& [i, x] : enumerate_view(std::move(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, v0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_lvalue_noref) {
    array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto x : a) { // NOLINT(performance-for-range-copy)
        static_assert(is_same_v<decltype(x), string>);
        EXPECT_NE(&x, a0_addr);
    }
    for (auto [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), string>);
        EXPECT_NE(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_lvalue_ref) {
    array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_lvalue_cref) {
    array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto const& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto const& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_lvalue_uref) {
    array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto&& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto&& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_const_lvalue_noref) {
    const array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto x : a) { // NOLINT(performance-for-range-copy)
        static_assert(is_same_v<decltype(x), string>);
        EXPECT_NE(&x, a0_addr);
    }
    for (auto [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), string>);
        EXPECT_NE(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_const_lvalue_ref) {
    const array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_const_lvalue_cref) {
    const array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto const& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto const& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_const_lvalue_uref) {
    const array<const string, 1> a{"abc"};
    auto a0_addr = &a[0];
    for (auto&& x : a) {
        static_assert(is_same_v<decltype(x), const string&>);
        EXPECT_EQ(&x, a0_addr);
    }
    for (auto&& [i, x] : enumerate_view(a)) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const string>);
        EXPECT_EQ(&x, a0_addr);
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_xvalue_noref) {
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto x : std::move(a)) { // NOLINT(performance-for-range-copy)
            static_assert(is_same_v<decltype(x), string>);
            EXPECT_NE(&x, a0_addr);
        }
    }
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto [i, x] : enumerate_view(std::move(a))) {
            static_assert(is_same_v<decltype(i), size_t>);
            static_assert(is_same_v<decltype(x), string>);
            EXPECT_NE(&x, a0_addr);
        }
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_xvalue_ref) {
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto& x : std::move(a)) {
            static_assert(is_same_v<decltype(x), const string&>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto& [i, x] : enumerate_view(std::move(a))) {
            static_assert(is_same_v<decltype(i), size_t>);
            static_assert(is_same_v<decltype(x), const string>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_xvalue_cref) {
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto const& x : std::move(a)) {
            static_assert(is_same_v<decltype(x), const string&>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto const& [i, x] : enumerate_view(std::move(a))) {
            static_assert(is_same_v<decltype(i), size_t>);
            static_assert(is_same_v<decltype(x), const string>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_const_elems_xvalue_uref) {
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto&& x : std::move(a)) {
            static_assert(is_same_v<decltype(x), const string&>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
    {
        array<const string, 1> a{"abc"};
        auto a0_addr = &a[0];
        for (auto&& [i, x] : enumerate_view(std::move(a))) {
            static_assert(is_same_v<decltype(i), size_t>);
            static_assert(is_same_v<decltype(x), const string>);
            EXPECT_EQ(&x, a0_addr);
        }
    }
}

namespace {
struct lifetime_tester {
    int* p_;
    explicit lifetime_tester(int* p)
    : p_(p) {}
    lifetime_tester(const lifetime_tester&) = delete;
    lifetime_tester(lifetime_tester&&) = delete;
    lifetime_tester& operator=(const lifetime_tester&) = delete;
    lifetime_tester& operator=(lifetime_tester&&) = delete;
    ~lifetime_tester() { *p_ = 42; }
};
} // namespace

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_xvalue_lifetime) {
    int lt = 0;
    for (auto&& x : vector{make_shared<lifetime_tester>(&lt)}) {
        static_assert(is_same_v<decltype(x), shared_ptr<lifetime_tester>&>);
        EXPECT_EQ(lt, 0);
    }
    EXPECT_EQ(lt, 42);

    lt = 0;
    for (auto&& [i, x] : enumerate_view(vector{make_shared<lifetime_tester>(&lt)})) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), shared_ptr<lifetime_tester>>);
        EXPECT_EQ(lt, 0);
    }
    EXPECT_EQ(lt, 42);
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_reverse_lvalue_noref) {
    vector<int> v{8, 4, 7};
    vector<pair<size_t, int>> res;
    for (auto [i, x] : enumerate_view(reverse_view(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_NE(&x, &v.rbegin()[i]);
        res.emplace_back(i, x);
    }
    EXPECT_EQ(res, (vector<pair<size_t, int>>{{0, 7}, {1, 4}, {2, 8}}));
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_reverse_lvalue_ref) {
    vector<int> v{8, 4, 7};
    vector<pair<size_t, int>> res;
    for (auto& [i, x] : enumerate_view(reverse_view(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, &v.rbegin()[i]);
        res.emplace_back(i, x);
    }
    EXPECT_EQ(res, (vector<pair<size_t, int>>{{0, 7}, {1, 4}, {2, 8}}));
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_reverse_lvalue_cref) {
    vector<int> v{8, 4, 7};
    vector<pair<size_t, int>> res;
    for (auto const& [i, x] : enumerate_view(reverse_view(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), const int>);
        EXPECT_EQ(&x, &v.rbegin()[i]);
        res.emplace_back(i, x);
    }
    EXPECT_EQ(res, (vector<pair<size_t, int>>{{0, 7}, {1, 4}, {2, 8}}));
}

// NOLINTNEXTLINE
TEST(ranges, enumerate_view_reverse_lvalue_uref) {
    vector<int> v{8, 4, 7};
    vector<pair<size_t, int>> res;
    for (auto&& [i, x] : enumerate_view(reverse_view(v))) {
        static_assert(is_same_v<decltype(i), size_t>);
        static_assert(is_same_v<decltype(x), int>);
        EXPECT_EQ(&x, &v.rbegin()[i]);
        res.emplace_back(i, x);
    }
    EXPECT_EQ(res, (vector<pair<size_t, int>>{{0, 7}, {1, 4}, {2, 8}}));
}
