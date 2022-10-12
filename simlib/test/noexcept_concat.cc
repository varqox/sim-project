#include "simlib/noexcept_concat.hh"
#include "simlib/string_view.hh"
#include "simlib/to_string.hh"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

// NOLINTNEXTLINE
TEST(noexcept_string_max_length, StaticCStringBuff) {
    static_assert(noexcept_string_max_length<StaticCStringBuff<0>> == 0);
    static_assert(noexcept_string_max_length<StaticCStringBuff<1>> == 1);
    static_assert(noexcept_string_max_length<StaticCStringBuff<42>> == 42);
}

// NOLINTNEXTLINE
TEST(noexcept_string_max_length, string_literal) {
    static_assert(noexcept_string_max_length<decltype("")> == 0);
    static_assert(noexcept_string_max_length<decltype("x")> == 1);
    static_assert(noexcept_string_max_length<decltype("a b c")> == 5);
    static_assert(noexcept_string_max_length<decltype("\0x\0y\0z")> == 6);
}

// NOLINTNEXTLINE
TEST(noexcept_string_length, StaticCStringBuff) {
    static_assert(noexcept_string_length(StaticCStringBuff<0>{}) == 0);
    static_assert(noexcept_string_length(StaticCStringBuff<1>{"x"}) == 1);
    static_assert(noexcept_string_length(StaticCStringBuff<42>{"a b c"}) == 5);
    static_assert(noexcept_string_length(StaticCStringBuff<42>{"a\0bc"}) == 1);
}

// NOLINTNEXTLINE
TEST(noexcept_string_length, string_literal) {
    static_assert(noexcept_string_length("") == 0);
    static_assert(noexcept_string_length("x") == 1);
    static_assert(noexcept_string_length("a b c") == 5);
    static_assert(noexcept_string_length("\0x\0y\0z") == 0);
    static_assert(noexcept_string_length("a\0bc") == 1);
}

// NOLINTNEXTLINE
TEST(is_noexcept_string_argument, valid) {
    static_assert(is_noexcept_string_argument<decltype("abc")>);
    static_assert(is_noexcept_string_argument<StaticCStringBuff<42>>);
    static_assert(is_noexcept_string_argument<StaticCStringBuff<42>&>);
    static_assert(is_noexcept_string_argument<StaticCStringBuff<42>&&>);
    static_assert(is_noexcept_string_argument<const StaticCStringBuff<42>&>);
    static_assert(is_noexcept_string_argument<int>);
    static_assert(is_noexcept_string_argument<size_t>);
    static_assert(is_noexcept_string_argument<uint64_t>);
}

// NOLINTNEXTLINE
TEST(is_noexcept_string_argument, invalid) {
    static_assert(!is_noexcept_string_argument<const char*>);
    static_assert(!is_noexcept_string_argument<std::nullptr_t>);
    static_assert(!is_noexcept_string_argument<StringView>);
    static_assert(!is_noexcept_string_argument<std::string>);
}

// NOLINTNEXTLINE
TEST(noexcept_concat, max_size) {
    static_assert(decltype(noexcept_concat())::max_size() == 0);
    static_assert(decltype(noexcept_concat("a", "", "b"))::max_size() == 2);
    static_assert(decltype(noexcept_concat("a", "b", "c"))::max_size() == 3);
    static_assert(decltype(noexcept_concat("\0", "a", "\0", "b", "\0"))::max_size() == 5);
    static_assert(decltype(noexcept_concat("xyz"))::max_size() == 3);
    static_assert(decltype(noexcept_concat("a", 42, "b"))::max_size() == 13);
    static_assert(decltype(noexcept_concat(noexcept_concat("a"), -1, "b"))::max_size() == 13);
    static_assert(decltype(noexcept_concat(StaticCStringBuff<4>("a"), "b", "c"))::max_size() == 6);
}

// NOLINTNEXTLINE
TEST(noexcept_concat, value) {
    static_assert(intentional_unsafe_string_view(noexcept_concat()).empty());
    static_assert(intentional_unsafe_string_view(noexcept_concat("a", "", "b")) == "ab");
    static_assert(intentional_unsafe_string_view(noexcept_concat("a", "b", "c")) == "abc");
    static_assert(
            intentional_unsafe_string_view(noexcept_concat("\0", "a", "\0", "b", "\0")) == "ab");
    static_assert(intentional_unsafe_string_view(noexcept_concat("xyz")) == "xyz");
    static_assert(intentional_unsafe_string_view(noexcept_concat("a", 42, "b")) == "a42b");
    static_assert(intentional_unsafe_string_view(noexcept_concat(noexcept_concat("a"), -1, "b")) ==
            "a-1b");
    static_assert(intentional_unsafe_string_view(
                          noexcept_concat(StaticCStringBuff<4>("a"), "b", "c")) == "abc");
}
