#include "simlib/concat_common.hh"
#include "simlib/string_view.hh"

#include <gtest/gtest.h>

using std::string;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_common, string_length) {
	EXPECT_EQ(string_length(string("abcdefghij")), 10);
	EXPECT_EQ(string_length("abcdefghij"), 10);
	char str[] = "abcdefghij";
	EXPECT_EQ(string_length(str), 10);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_common, string_length_with_null) {
	EXPECT_EQ(string_length(string("abcd\0efghij", 10)), 10);
	EXPECT_EQ(string_length("abcd\0efghij"), 4);
	char str[] = "abcd\0efghij";
	EXPECT_EQ(string_length(str), 4);
}

template <class Str>
static StringView wrap(const Str& str) noexcept {
	return {::data(str), string_length(str)};
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_common, data) {
	EXPECT_EQ(wrap(string("abcdefghij")), string("abcdefghij"));
	EXPECT_EQ(wrap("abcdefghij"), "abcdefghij");
	char str[] = "abcdefghij";
	EXPECT_EQ(wrap(str), str);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_common, data_with_null) {
	EXPECT_EQ(wrap(string("abcd\0efghij", 10)), string("abcd\0efghij", 10));
	EXPECT_EQ(wrap("abcd\0efghij"), "abcd");
	char str[] = "abcd\0efghij";
	EXPECT_EQ(wrap(str), "abcd");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_common, stringify) {
	EXPECT_EQ(wrap(stringify(string("abcdefghij"))), "abcdefghij");
	EXPECT_EQ(wrap(stringify("abcdefghij")), "abcdefghij");
	char str[] = "abcdefghij";
	EXPECT_EQ(wrap(stringify(str)), "abcdefghij");
	EXPECT_EQ(
		wrap(stringify(string("abcd\0efghij", 10))),
		string("abcd\0efghij", 10));
	EXPECT_EQ(wrap(stringify("abcd\0efghij")), "abcd");
	char str_with_null[] = "abcd\0efghij";
	EXPECT_EQ(wrap(stringify(str_with_null)), "abcd");

	EXPECT_EQ(wrap(stringify('a')), "a");
	EXPECT_EQ(wrap(stringify('\n')), "\n");
	EXPECT_EQ(wrap(stringify('\0')), string{'\0'});

	EXPECT_EQ(wrap(stringify(1234567890123456789)), "1234567890123456789");
	EXPECT_EQ(wrap(stringify(123456789)), "123456789");
	EXPECT_EQ(wrap(stringify(0)), "0");
	EXPECT_EQ(wrap(stringify(-0)), "0");
	EXPECT_EQ(wrap(stringify(-1256)), "-1256");
	EXPECT_EQ(wrap(stringify(-1234567890123456789)), "-1234567890123456789");
}
