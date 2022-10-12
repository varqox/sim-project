#include "simlib/strongly_typed_function.hh"

#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

static void foo(int /*unused*/) {}
static void noexcept_foo(int /*unused*/) noexcept {}
template <class T>
static void bar(T /*unused*/) {}
template <class T>
static void noexcept_bar(T /*unused*/) noexcept {}

void uint_foo(uint /*unused*/);
void uint_noexcept_foo(uint /*unused*/);

// NOLINTNEXTLINE
TEST(strongly_typed_function, constructor_checks_arguments) {
    (void)strongly_typed_function<void(int)>(foo);
    (void)strongly_typed_function<void(int)>(noexcept_foo);
    (void)strongly_typed_function<void(int)>(bar);
    (void)strongly_typed_function<void(int)>(noexcept_bar);

    (void)strongly_typed_function<void(int)>([&](int /*unused*/) {});
    (void)strongly_typed_function<void(int)>([&](int /*unused*/) mutable {});
    (void)strongly_typed_function<void(int)>([&](int /*unused*/) noexcept {});
    (void)strongly_typed_function<void(int)>([&](int /*unused*/) mutable noexcept {});
    (void)strongly_typed_function<void(int)>([&](auto /*unused*/) {});
    (void)strongly_typed_function<void(int)>([&](auto /*unused*/) mutable {});
    (void)strongly_typed_function<void(int)>([&](auto /*unused*/) noexcept {});
    (void)strongly_typed_function<void(int)>([&](auto /*unused*/) mutable noexcept {});

    static_assert(
            not std::is_constructible_v<strongly_typed_function<void(int)>, decltype(uint_foo)>);
    static_assert(not std::is_constructible_v<strongly_typed_function<void(int)>,
                  decltype(uint_noexcept_foo)>);

    auto lambda = [&](uint /*unused*/) {};
    static_assert(
            not std::is_constructible_v<strongly_typed_function<void(int)>, decltype(lambda)>);
    auto lambda_mut = [&](uint /*unused*/) mutable {};
    static_assert(
            not std::is_constructible_v<strongly_typed_function<void(int)>, decltype(lambda_mut)>);
}

static int ret_int() { return -42; }
static int ret_int_noexcept() noexcept { return -42; }
uint ret_uint();
uint ret_uint_noexcept() noexcept;

// NOLINTNEXTLINE
TEST(strongly_typed_function, constructor_checks_return_value) {
    (void)strongly_typed_function<int()>(ret_int);
    (void)strongly_typed_function<int()>(ret_int_noexcept);
    (void)strongly_typed_function<int()>([&] { return 17; });
    (void)strongly_typed_function<int()>([&] { return 17; });
    (void)strongly_typed_function<int()>([&]() noexcept { return 17; });
    (void)strongly_typed_function<int()>([&]() mutable { return 17; });
    (void)strongly_typed_function<int()>([&]() mutable noexcept { return 17; });

    static_assert(not std::is_constructible_v<strongly_typed_function<int()>, decltype(ret_uint)>);
    static_assert(not std::is_constructible_v<strongly_typed_function<int()>,
                  decltype(ret_uint_noexcept)>);

    auto lambda = [&]() -> uint { return 17; };
    static_assert(not std::is_constructible_v<strongly_typed_function<int()>, decltype(lambda)>);
    auto lambda_mut = [&]() mutable -> uint { return 17; };
    static_assert(
            not std::is_constructible_v<strongly_typed_function<int()>, decltype(lambda_mut)>);
}

// NOLINTNEXTLINE
TEST(strongly_typed_function, call) {
    strongly_typed_function<int64_t(int64_t, int32_t)> func(
            [](int64_t x, int32_t y) { return x - y; });
    EXPECT_EQ(func(4, 8), -4);
}
