#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <simlib/to_string.hh>

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
TEST(to_string, to_string_with_integral) {
    static_assert(to_string(false) == "false");
    static_assert(to_string(true) == "true");

    static_assert(to_string(static_cast<char>(48)) == "0");
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(to_string(static_cast<char>(0)) == "\0");

    static_assert(to_string(std::numeric_limits<int8_t>::min()) == "-128");
    static_assert(to_string(static_cast<int8_t>(-107)) == "-107");
    static_assert(to_string(static_cast<int8_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int8_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int8_t>(0)) == "0");
    static_assert(to_string(static_cast<int8_t>(1)) == "1");
    static_assert(to_string(static_cast<int8_t>(42)) == "42");
    static_assert(to_string(static_cast<int8_t>(118)) == "118");
    static_assert(to_string(std::numeric_limits<int8_t>::max()) == "127");
    static_assert(to_string(std::numeric_limits<uint8_t>::min()) == "0");
    static_assert(to_string(static_cast<uint8_t>(0)) == "0");
    static_assert(to_string(static_cast<uint8_t>(1)) == "1");
    static_assert(to_string(static_cast<uint8_t>(42)) == "42");
    static_assert(to_string(static_cast<uint8_t>(239)) == "239");
    static_assert(to_string(std::numeric_limits<uint8_t>::max()) == "255");

    static_assert(to_string(std::numeric_limits<int16_t>::min()) == "-32768");
    static_assert(to_string(static_cast<int16_t>(-26358)) == "-26358");
    static_assert(to_string(static_cast<int16_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int16_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int16_t>(0)) == "0");
    static_assert(to_string(static_cast<int16_t>(1)) == "1");
    static_assert(to_string(static_cast<int16_t>(42)) == "42");
    static_assert(to_string(static_cast<int16_t>(16356)) == "16356");
    static_assert(to_string(std::numeric_limits<int16_t>::max()) == "32767");
    static_assert(to_string(std::numeric_limits<uint16_t>::min()) == "0");
    static_assert(to_string(static_cast<uint16_t>(0)) == "0");
    static_assert(to_string(static_cast<uint16_t>(1)) == "1");
    static_assert(to_string(static_cast<uint16_t>(42)) == "42");
    static_assert(to_string(static_cast<uint16_t>(53621)) == "53621");
    static_assert(to_string(std::numeric_limits<uint16_t>::max()) == "65535");

    static_assert(to_string(std::numeric_limits<int32_t>::min()) == "-2147483648");
    static_assert(to_string(static_cast<int32_t>(-1642516759)) == "-1642516759");
    static_assert(to_string(static_cast<int32_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int32_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int32_t>(0)) == "0");
    static_assert(to_string(static_cast<int32_t>(1)) == "1");
    static_assert(to_string(static_cast<int32_t>(42)) == "42");
    static_assert(to_string(static_cast<int32_t>(2136532621)) == "2136532621");
    static_assert(to_string(std::numeric_limits<int32_t>::max()) == "2147483647");
    static_assert(to_string(std::numeric_limits<uint32_t>::min()) == "0");
    static_assert(to_string(static_cast<uint32_t>(0)) == "0");
    static_assert(to_string(static_cast<uint32_t>(1)) == "1");
    static_assert(to_string(static_cast<uint32_t>(42)) == "42");
    static_assert(to_string(static_cast<uint32_t>(3643551363)) == "3643551363");
    static_assert(to_string(std::numeric_limits<uint32_t>::max()) == "4294967295");

    static_assert(to_string(std::numeric_limits<int64_t>::min()) == "-9223372036854775808");
    static_assert(to_string(static_cast<int64_t>(-6452185143265312634)) == "-6452185143265312634");
    static_assert(to_string(static_cast<int64_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int64_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int64_t>(0)) == "0");
    static_assert(to_string(static_cast<int64_t>(1)) == "1");
    static_assert(to_string(static_cast<int64_t>(42)) == "42");
    static_assert(to_string(static_cast<int64_t>(6421358215683281653)) == "6421358215683281653");
    static_assert(to_string(std::numeric_limits<int64_t>::max()) == "9223372036854775807");
    static_assert(to_string(std::numeric_limits<uint64_t>::min()) == "0");
    static_assert(to_string(static_cast<uint64_t>(0)) == "0");
    static_assert(to_string(static_cast<uint64_t>(1)) == "1");
    static_assert(to_string(static_cast<uint64_t>(42)) == "42");
    static_assert(
        to_string(static_cast<uint64_t>(16421358215683281653ULL)) == "16421358215683281653"
    );
    static_assert(to_string(std::numeric_limits<uint64_t>::max()) == "18446744073709551615");
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
