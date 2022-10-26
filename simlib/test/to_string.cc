#include "simlib/to_string.hh"

#include <gtest/gtest.h>

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, default_constructor) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, constructor_from_std_array_with_len) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, constructor_from_std_array) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, copy_constructor) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, move_constructor) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(StaticCStringBuff, constructor_from_const_char_array) {
    static_assert(StaticCStringBuff<3>("abc") == "abc");
    static_assert(StaticCStringBuff<42>("abc") == "abc");
    static_assert(StaticCStringBuff<42>("") == "");
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(StaticCStringBuff<42>("\0a\0b\0c") == "\0a\0b\0c");
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(StaticCStringBuff<1>("\0") == "\0");
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, template_copy_constructor) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, copy_assignment) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, move_assignment) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, size) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(StaticCStringBuff, max_size) {
    static_assert(StaticCStringBuff<0>::max_size() == 0);
    static_assert(StaticCStringBuff<1>::max_size() == 1);
    static_assert(StaticCStringBuff<2>::max_size() == 2);
    static_assert(StaticCStringBuff<42>::max_size() == 42);
    static_assert(StaticCStringBuff<128>::max_size() == 128);
    static_assert(StaticCStringBuff<142>::max_size() == 142);
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, data) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, c_str) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, begin) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, end) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_StaticCStringBuff, operator_subscript) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(StaticCStringBuff, operator_equals) {
    static_assert(StaticCStringBuff<3>("abc") == "abc");
    static_assert(!(StaticCStringBuff<3>("abc") == "abd"));
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(StaticCStringBuff<4>("ab\0c") == "ab\0c");
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(!(StaticCStringBuff<4>("ab\0c") == "ab\0d"));
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_char) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_bool) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_floating_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_floating_point_with_precision) {
    // TODO: implement it
}
