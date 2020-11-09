#include "simlib/concat.hh"

#include <gtest/gtest.h>

using std::string;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat, concat) {
	EXPECT_EQ("", concat(""));
	EXPECT_EQ("", concat("", "", "", ""));
	EXPECT_EQ("ab c de", concat("a", 'b', ' ', "c ", "de"));
	EXPECT_EQ(
		"ab c de", concat("a", 'b', ' ', CStringView("c "), string("de")));
	EXPECT_EQ(
		StringView("a\0\0abc", 6), concat('a', '\0', '\0', StringView("abc")));

	EXPECT_NE(StringView("a\0\0abc", 6), concat('a', '\0', '\0', "abcd"));

	EXPECT_EQ(
		"abc true 0 1 -1 2 3 -3 false",
		concat(
			"abc ", true, " ", 0, ' ', 1, ' ', -1, " ", 2, ' ', 3, ' ', -3, ' ',
			false));

	EXPECT_EQ(
		" bac-1234567890123456789\t",
		concat(" bac", -1234567890123456789, '\t'));
}
