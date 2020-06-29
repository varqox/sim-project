#include "simlib/concat_tostr.hh"
#include "simlib/meta.hh"
#include "simlib/string_view.hh"

#include <gtest/gtest.h>
#include <utility>

using std::string;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_tostr, concat_tostr) {
	EXPECT_EQ("", concat_tostr(""));
	EXPECT_EQ("", concat_tostr("", "", "", ""));
	EXPECT_EQ("ab c de", concat_tostr("a", 'b', ' ', "c ", "de"));
	EXPECT_EQ("ab c de",
	          concat_tostr("a", 'b', ' ', CStringView("c "), string("de")));
	EXPECT_EQ(StringView("a\0\0abc", 6),
	          concat_tostr('a', '\0', '\0', StringView("abc")));

	EXPECT_NE(StringView("a\0\0abc", 6), concat_tostr('a', '\0', '\0', "abcd"));

	EXPECT_EQ("abc true 0 1 -1 2 3 -3 false",
	          concat_tostr("abc ", true, " ", 0, ' ', 1, ' ', -1, " ", 2, ' ',
	                       3, ' ', -3, ' ', false));

	EXPECT_EQ(" bac-1234567890123456789\t",
	          concat_tostr(" bac", -1234567890123456789, '\t'));
}

template <size_t... Idx1, size_t... Idx2, class... Args>
static string
concat_and_back_insert_impl(std::tuple<Args&&...> args,
                            std::index_sequence<Idx1...> /*unused*/,
                            std::index_sequence<Idx2...> /*unused*/) {
	string str = concat_tostr(std::get<Idx1>(args)...);
	back_insert(str, std::get<Idx2>(args)...);
	return str;
}

// Splits args into two parts left and right (left has LEFT_ARGS_NO elements)
// and returns back_insert(concat_tostr(left), right)
template <size_t LEFT_ARGS_NO = 0, class... Args>
static string concat_and_back_insert(Args&... args) {
	return concat_and_back_insert_impl(
	   std::forward_as_tuple(std::forward<Args>(args)...),
	   meta::span<0, LEFT_ARGS_NO>(),
	   meta::span<LEFT_ARGS_NO, sizeof...(args)>());
}

// For every split of args into two parts left and right checks that
// expected == back_insert(concat_tostr(left), right)
template <size_t LEFT_ARGS_NO = 0, class T, class... Args>
static void test_back_insert_on(int line, T&& expected, Args&&... args) {
	EXPECT_EQ(expected, concat_and_back_insert<LEFT_ARGS_NO>(args...))
	   << "line: " << line;

	if constexpr (LEFT_ARGS_NO < sizeof...(args)) {
		test_back_insert_on<LEFT_ARGS_NO + 1>(line, expected, args...);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(concat_tostr, back_insert) {
	test_back_insert_on(__LINE__, "", "");
	test_back_insert_on(__LINE__, "", "", "", "", "");
	test_back_insert_on(__LINE__, "ab c de", "a", 'b', ' ', "c ", "de");
	test_back_insert_on(__LINE__, "ab c de", "a", 'b', ' ', CStringView("c "),
	                    string("de"));
	test_back_insert_on(__LINE__, StringView("a\0\0abc", 6), 'a', '\0', '\0',
	                    StringView("abc"));

	test_back_insert_on(__LINE__, "abc true 0 1 -1 2 3 -3 false", "abc ", true,
	                    " ", 0, ' ', 1, ' ', -1, " ", 2, ' ', 3, ' ', -3, ' ',
	                    false);

	test_back_insert_on(__LINE__, " bac-1234567890123456789\t", " bac",
	                    -1234567890123456789, '\t');
}
