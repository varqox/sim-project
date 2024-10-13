#include <gtest/gtest.h>
#include <simlib/escape_bytes_to_utf8_str.hh>

TEST(escape_bytes_to_utf8_str, simple) {
    EXPECT_EQ(
        escape_bytes_to_utf8_str("'", "abcdefghijklmnopqrstuwvxyz ABCDEFGHIJKLMNOPQRSTUWVXYZ", "'"),
        "'abcdefghijklmnopqrstuwvxyz ABCDEFGHIJKLMNOPQRSTUWVXYZ'"
    );
}

TEST(escape_bytes_to_utf8_str, utf8) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", "ąćęłńóśźż", ""), "ąćęłńóśźż");
}

TEST(escape_bytes_to_utf8_str, valid_utf8_2_byte) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xc0\x80", ""), "\xc0\x80");
}

TEST(escape_bytes_to_utf8_str, valid_utf8_3_byte) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xe0\x80\x80", ""), "\xe0\x80\x80");
}

TEST(escape_bytes_to_utf8_str, valid_utf8_4_byte) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x80\x80\x80", ""), "\xf0\x80\x80\x80");
}

TEST(escape_bytes_to_utf8_str, invalid_utf8_2_byte) {
    // sequence is too short
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xc0", ""), "\\xc0");
    // second byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xc0\x7f", ""), "\\xc0\\x7f");
}

TEST(escape_bytes_to_utf8_str, invalid_utf8_3_byte) {
    // sequence is too short
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xe0", ""), "\\xe0");
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xe0\x80", ""), "\\xe0\\x80");
    // second byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xe0\x7f\x80", ""), "\\xe0\\x7f\\x80");
    // third byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xe0\x80\x7f", ""), "\\xe0\\x80\\x7f");
}

TEST(escape_bytes_to_utf8_str, invalid_utf8_4_byte) {
    // sequence is too short
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0", ""), "\\xf0");
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x80", ""), "\\xf0\\x80");
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x80\x80", ""), "\\xf0\\x80\\x80");
    // second byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x7f\x80\x80", ""), "\\xf0\\x7f\\x80\\x80");
    // third byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x80\x7f\x80", ""), "\\xf0\\x80\\x7f\\x80");
    // fourth byte is invalid
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\xf0\x80\x80\x7f", ""), "\\xf0\\x80\\x80\\x7f");
}

TEST(escape_bytes_to_utf8_str, control_characters) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", {"\n\t\r\0", 4}, ""), "\\n\\t\\r\\0");
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x01\x02\x03\x04\x05\x06\x07", ""),
        "\\x01\\x02\\x03\\x04\\x05\\x06\\x07"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", ""),
        "\\x08\\t\\n\\x0b\\x0c\\r\\x0e\\x0f"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x10\x11\x12\x13\x14\x15\x16\x17", ""),
        "\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f", ""),
        "\\x18\\x19\\x1a\\x1b\\x1c\\x1d\\x1e\\x1f"
    );
}

TEST(escape_bytes_to_utf8_str, del) {
    EXPECT_EQ(escape_bytes_to_utf8_str("", "\x7f", ""), "\\x7f");
}

TEST(escape_bytes_to_utf8_str, continuation_bytes_without_the_first_byte) {
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x80\x81\x82\x83\x84\x85\x86\x87", ""),
        "\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f", ""),
        "\\x88\\x89\\x8a\\x8b\\x8c\\x8d\\x8e\\x8f"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x90\x91\x92\x93\x94\x95\x96\x97", ""),
        "\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f", ""),
        "\\x98\\x99\\x9a\\x9b\\x9c\\x9d\\x9e\\x9f"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7", ""),
        "\\xa0\\xa1\\xa2\\xa3\\xa4\\xa5\\xa6\\xa7"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\xa8\xa9\xaa\xab\xac\xad\xae\xaf", ""),
        "\\xa8\\xa9\\xaa\\xab\\xac\\xad\\xae\\xaf"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7", ""),
        "\\xb0\\xb1\\xb2\\xb3\\xb4\\xb5\\xb6\\xb7"
    );
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf", ""),
        "\\xb8\\xb9\\xba\\xbb\\xbc\\xbd\\xbe\\xbf"
    );
}

TEST(escape_bytes_to_utf8_str, other_invalid_bytes) {
    EXPECT_EQ(
        escape_bytes_to_utf8_str("", "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff", ""),
        "\\xf8\\xf9\\xfa\\xfb\\xfc\\xfd\\xfe\\xff"
    );
}
