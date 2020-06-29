#include "simlib/string_transform.hh"

#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, has_prefix) {
	EXPECT_EQ(true, has_prefix("", ""));
	EXPECT_EQ(true, has_prefix("abc", ""));
	EXPECT_EQ(true, has_prefix("abc", "a"));
	EXPECT_EQ(true, has_prefix("abc", "ab"));
	EXPECT_EQ(true, has_prefix("abc", "abc"));

	EXPECT_EQ(false, has_prefix("abc", "bc"));
	EXPECT_EQ(false, has_prefix("abc", "b"));
	EXPECT_EQ(false, has_prefix("abc", "c"));
	EXPECT_EQ(false, has_prefix("abc", "ac"));
	EXPECT_EQ(false, has_prefix("", "a"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, has_one_of_prefixes) {
	EXPECT_EQ(true, has_one_of_prefixes("", "abc", "b", ""));
	EXPECT_EQ(true, has_one_of_prefixes("abc", "bc", "b", "", "c", "ac"));
	EXPECT_EQ(true, has_one_of_prefixes("abc", "bc", "b", "a", "c", "ac"));
	EXPECT_EQ(true, has_one_of_prefixes("abc", "bc", "b", "ab", "c", "ac"));
	EXPECT_EQ(true, has_one_of_prefixes("abc", "bc", "b", "abc", "c", "ac"));

	EXPECT_EQ(false, has_one_of_prefixes("abc", "bc", "b", "c", "ac"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, has_suffix) {
	EXPECT_EQ(true, has_suffix("", ""));
	EXPECT_EQ(true, has_suffix("cba", ""));
	EXPECT_EQ(true, has_suffix("cba", "a"));
	EXPECT_EQ(true, has_suffix("cba", "ba"));
	EXPECT_EQ(true, has_suffix("cba", "cba"));

	EXPECT_EQ(false, has_suffix("cba", "cb"));
	EXPECT_EQ(false, has_suffix("cba", "b"));
	EXPECT_EQ(false, has_suffix("cba", "c"));
	EXPECT_EQ(false, has_suffix("cba", "ca"));
	EXPECT_EQ(false, has_suffix("", "a"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, has_one_of_suffixes) {
	EXPECT_EQ(true, has_one_of_suffixes("", "cba", "b", ""));
	EXPECT_EQ(true, has_one_of_suffixes("cba", "cb", "b", "", "c", "ca"));
	EXPECT_EQ(true, has_one_of_suffixes("cba", "cb", "b", "a", "c", "ca"));
	EXPECT_EQ(true, has_one_of_suffixes("cba", "cb", "b", "ba", "c", "ca"));
	EXPECT_EQ(true, has_one_of_suffixes("cba", "cb", "b", "cba", "c", "ca"));

	EXPECT_EQ(false, has_one_of_suffixes("cba", "cb", "b", "c", "ca"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_digit) {
	EXPECT_EQ(true, is_digit("0"));
	EXPECT_EQ(true, is_digit("01"));
	EXPECT_EQ(true, is_digit("0123456789"));

	EXPECT_EQ(false, is_digit(""));
	EXPECT_EQ(false, is_digit("-"));
	EXPECT_EQ(false, is_digit("+123456789"));
	EXPECT_EQ(false, is_digit("--123456789"));
	EXPECT_EQ(false, is_digit("-123456-789"));
	EXPECT_EQ(false, is_digit("-123456789"));
	EXPECT_EQ(false, is_digit("-12345a6789"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_alpha) {
	EXPECT_EQ(true,
	          is_alpha("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));

	EXPECT_EQ(false, is_alpha(""));
	EXPECT_EQ(
	   false,
	   is_alpha("abcdefghijklmnopqrstuvwxyzABCDEFGHIJK-LMNOPQRSTUVWXYZ"));
	EXPECT_EQ(
	   false,
	   is_alpha("abcdefghijklmnopqrstuvwxyzABCDEFGHIJK0LMNOPQRSTUVWXYZ"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_alnum) {
	EXPECT_EQ(
	   true,
	   is_alnum(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"));

	EXPECT_EQ(false, is_alnum(""));
	EXPECT_EQ(
	   false,
	   is_alnum(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGH-IJKLMNOPQRSTUVWXYZ"));
	EXPECT_EQ(
	   false,
	   is_alnum(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFG#IJKLMNOPQRSTUVWXYZ"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_word) {
	for (int c = 0; c < 256; ++c) {
		EXPECT_EQ(bool(is_alnum(c) or c == '-' or c == '_'), is_word(c));
	}

	EXPECT_EQ(
	   true,
	   is_word(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-_"));

	EXPECT_EQ(false, is_word(""));
	EXPECT_EQ(
	   false,
	   is_word(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ@KLMNOPQRSTUVWXYZ-_"));
	EXPECT_EQ(
	   false,
	   is_word(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ#KLMNOPQRSTUVWXYZ-_"));
	EXPECT_EQ(
	   false,
	   is_word(
	      "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ`KLMNOPQRSTUVWXYZ-_"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_integer) {
	EXPECT_EQ(true, is_integer("-1234567890"));
	EXPECT_EQ(true, is_integer("0"));
	EXPECT_EQ(true, is_integer("01"));

	EXPECT_EQ(false, is_integer(""));
	EXPECT_EQ(false, is_integer("+1234567890"));
	EXPECT_EQ(false, is_integer("-"));
	EXPECT_EQ(false, is_integer("--1234567890"));
	EXPECT_EQ(false, is_integer("-123456-7890"));
	EXPECT_EQ(false, is_integer("-12345a67890"));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(string_traits, is_real) {
	EXPECT_EQ(true, is_real("-1234567890"));
	EXPECT_EQ(true, is_real("0"));
	EXPECT_EQ(true, is_real("01"));

	EXPECT_EQ(false, is_real(""));
	EXPECT_EQ(false, is_real("+1234567890"));
	EXPECT_EQ(false, is_real("-"));
	EXPECT_EQ(false, is_real("--1234567890"));
	EXPECT_EQ(false, is_real("-123456-7890"));
	EXPECT_EQ(false, is_real("-12345a67890"));

	EXPECT_EQ(true, is_real("1.1"));
	EXPECT_EQ(true, is_real("0123456789.0123456789"));
	EXPECT_EQ(true, is_real("0000.00000"));

	EXPECT_EQ(false, is_real("."));
	EXPECT_EQ(false, is_real(".."));
	EXPECT_EQ(false, is_real(".1"));
	EXPECT_EQ(false, is_real("1."));
	EXPECT_EQ(false, is_real("1.1."));
	EXPECT_EQ(false, is_real("1.1.1"));
	EXPECT_EQ(false, is_real(".1."));
	EXPECT_EQ(false, is_real(".1.1"));
}
