#include "../include/string_transform.hh"

#include <gtest/gtest.h>

TEST(DISABLED_string_transform, tolower) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, hex2dec) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, dec2Hex) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, dec2hex) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, to_hex) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, encode_uri) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, decode_uri) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, append_as_html_escaped_with_with_char) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, append_as_html_escaped_with_with_StringView) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, html_escape) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, json_stringify) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, str2num_integral) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, str2num_floating_point) {
	// TODO: implement it
}

TEST(DISABLED_string_transform, str2num_with_bounds) {
	// TODO: implement it
}

TEST(string_trasform, padded_string) {
	EXPECT_EQ("  abc", padded_string("abc", 5));
	EXPECT_EQ("abc  ", padded_string("abc", 5, LEFT));
	EXPECT_EQ("0001234", padded_string("1234", 7, RIGHT, '0'));
	EXPECT_EQ("1234", padded_string("1234", 4, RIGHT, '0'));
	EXPECT_EQ("1234", padded_string("1234", 2, RIGHT, '0'));
}
