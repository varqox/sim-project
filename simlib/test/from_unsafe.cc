#include <gtest/gtest.h>
#include <simlib/from_unsafe.hh>
#include <type_traits>

struct NoCopyNoMove {
    NoCopyNoMove() noexcept = default;
    NoCopyNoMove(const NoCopyNoMove&) noexcept = default;
    NoCopyNoMove(NoCopyNoMove&&) noexcept = default;
    NoCopyNoMove& operator=(const NoCopyNoMove&) noexcept = default;
    NoCopyNoMove& operator=(NoCopyNoMove&&) noexcept = default;
    ~NoCopyNoMove() = default;
};

// NOLINTNEXTLINE
TEST(from_unsafe, get) {
    auto u = from_unsafe{1, "abc", NoCopyNoMove{}};
    static_assert(std::is_constructible_v<from_unsafe<NoCopyNoMove>, NoCopyNoMove&&>);
    static_assert(std::is_same_v<decltype(u.get<0>()), int&&>);
    static_assert(std::is_same_v<decltype(u.get<1>()), const char(&&)[4]>);
    static_assert(std::is_same_v<decltype(u.get<2>()), NoCopyNoMove&&>);
}

// NOLINTNEXTLINE
TEST(from_unsafe, get_ref) {
    auto u = from_unsafe{1, "abc", NoCopyNoMove{}};
    static_assert(std::is_same_v<decltype(u.get_ref<0>()), int&>);
    static_assert(std::is_same_v<decltype(u.get_ref<1>()), const char(&)[4]>);
    static_assert(std::is_same_v<decltype(u.get_ref<2>()), NoCopyNoMove&>);
}

// NOLINTNEXTLINE
TEST(from_unsafe, copy_move_semantics) {
    auto u = from_unsafe{};
    static_assert(!std::is_copy_constructible_v<decltype(u)>);
    static_assert(!std::is_move_constructible_v<decltype(u)>);
    static_assert(!std::is_copy_assignable_v<decltype(u)>);
    static_assert(!std::is_move_assignable_v<decltype(u)>);
}
