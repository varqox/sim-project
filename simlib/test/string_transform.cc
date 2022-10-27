#include <gtest/gtest.h>
#include <simlib/string_transform.hh>

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, to_lower) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(string_transform, hex2dec) {
    EXPECT_EQ(0, hex2dec('0'));
    EXPECT_EQ(1, hex2dec('1'));
    EXPECT_EQ(2, hex2dec('2'));
    EXPECT_EQ(3, hex2dec('3'));
    EXPECT_EQ(4, hex2dec('4'));
    EXPECT_EQ(5, hex2dec('5'));
    EXPECT_EQ(6, hex2dec('6'));
    EXPECT_EQ(7, hex2dec('7'));
    EXPECT_EQ(8, hex2dec('8'));
    EXPECT_EQ(9, hex2dec('9'));

    EXPECT_EQ(10, hex2dec('A'));
    EXPECT_EQ(11, hex2dec('B'));
    EXPECT_EQ(12, hex2dec('C'));
    EXPECT_EQ(13, hex2dec('D'));
    EXPECT_EQ(14, hex2dec('E'));
    EXPECT_EQ(15, hex2dec('F'));

    EXPECT_EQ(10, hex2dec('a'));
    EXPECT_EQ(11, hex2dec('b'));
    EXPECT_EQ(12, hex2dec('c'));
    EXPECT_EQ(13, hex2dec('d'));
    EXPECT_EQ(14, hex2dec('e'));
    EXPECT_EQ(15, hex2dec('f'));
}

// NOLINTNEXTLINE
TEST(string_transform_DeathTest, hex2dec) {
    ASSERT_DEATH({ hex2dec('{'); }, "Assertion.*failed");
    ASSERT_DEATH({ hex2dec('G'); }, "Assertion.*failed");
}

// NOLINTNEXTLINE
TEST(string_transform, dec2Hex) {
    EXPECT_EQ('0', dec2Hex(0));
    EXPECT_EQ('1', dec2Hex(1));
    EXPECT_EQ('2', dec2Hex(2));
    EXPECT_EQ('3', dec2Hex(3));
    EXPECT_EQ('4', dec2Hex(4));
    EXPECT_EQ('5', dec2Hex(5));
    EXPECT_EQ('6', dec2Hex(6));
    EXPECT_EQ('7', dec2Hex(7));
    EXPECT_EQ('8', dec2Hex(8));
    EXPECT_EQ('9', dec2Hex(9));
    EXPECT_EQ('A', dec2Hex(10));
    EXPECT_EQ('B', dec2Hex(11));
    EXPECT_EQ('C', dec2Hex(12));
    EXPECT_EQ('D', dec2Hex(13));
    EXPECT_EQ('E', dec2Hex(14));
    EXPECT_EQ('F', dec2Hex(15));
}

// NOLINTNEXTLINE
TEST(string_transform_DeathTest, dec2Hex) {
    ASSERT_DEATH({ dec2Hex(-1); }, "Assertion.*failed");
    ASSERT_DEATH({ dec2Hex(16); }, "Assertion.*failed");
}

// NOLINTNEXTLINE
TEST(string_transform, dec2hex) {
    EXPECT_EQ('0', dec2hex(0));
    EXPECT_EQ('1', dec2hex(1));
    EXPECT_EQ('2', dec2hex(2));
    EXPECT_EQ('3', dec2hex(3));
    EXPECT_EQ('4', dec2hex(4));
    EXPECT_EQ('5', dec2hex(5));
    EXPECT_EQ('6', dec2hex(6));
    EXPECT_EQ('7', dec2hex(7));
    EXPECT_EQ('8', dec2hex(8));
    EXPECT_EQ('9', dec2hex(9));
    EXPECT_EQ('a', dec2hex(10));
    EXPECT_EQ('b', dec2hex(11));
    EXPECT_EQ('c', dec2hex(12));
    EXPECT_EQ('d', dec2hex(13));
    EXPECT_EQ('e', dec2hex(14));
    EXPECT_EQ('f', dec2hex(15));
}

// NOLINTNEXTLINE
TEST(string_transform_DeathTest, dec2hex) {
    ASSERT_DEATH({ dec2hex(-1); }, "Assertion.*failed");
    ASSERT_DEATH({ dec2hex(17); }, "Assertion.*failed");
}

// NOLINTNEXTLINE
TEST(string_transform, to_hex) {
    EXPECT_EQ("4200baddab", to_hex(StringView("\x42\0\xba\xdd\xab", 5)));
    EXPECT_EQ("92ffabc0ff1e0d200a20096162636465666768696a6b6c6d6e6f707172737475767778"
              "797a",
            to_hex("\x92\xff\xab\xc0\xff\x1e\r \n \tabcdefghijklmnopqrstuvwxyz"));

    for (int i = 0; i < 256; ++i) {
        auto char_i = static_cast<char>(i);
        const std::array expected = {dec2hex(i >> 4), dec2hex(i & 15)};
        EXPECT_EQ(StringView(expected.data(), 2), to_hex(StringView(&char_i, 1)))
                << " for i = " << i;
    }
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, encode_uri) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, decode_uri) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, append_as_html_escaped_with_with_char) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, append_as_html_escaped_with_with_StringView) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, html_escape) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, json_stringify) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, str2num_integral) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, str2num_floating_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_transform, str2num_with_bounds) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(string_trasform, padded_string) {
    EXPECT_EQ("  abc", padded_string("abc", 5));
    EXPECT_EQ("abc  ", padded_string("abc", 5, LEFT));
    EXPECT_EQ("0001234", padded_string("1234", 7, RIGHT, '0'));
    EXPECT_EQ("1234", padded_string("1234", 4, RIGHT, '0'));
    EXPECT_EQ("1234", padded_string("1234", 2, RIGHT, '0'));
}
