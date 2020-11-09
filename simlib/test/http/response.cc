#include "simlib/http/response.hh"

#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(http, quote) {
	using http::quote;

	EXPECT_EQ(quote(""), "\"\"");
	EXPECT_EQ(
		quote("abcdefghijklmnopqrstuvwxyz"), "\"abcdefghijklmnopqrstuvwxyz\"");
	EXPECT_EQ(quote("\""), "\"\\\"\"");
	EXPECT_EQ(
		quote("\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""),
		"\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\"");
	EXPECT_EQ(quote("\n"), "\"\\\n\"");
	EXPECT_EQ(quote("a\tb"), "\"a\\\tb\"");
	EXPECT_EQ(quote("a\"b"), "\"a\\\"b\"");
}
