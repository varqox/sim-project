#include "simlib/ctype.hh"

#include <gtest/gtest.h>

constexpr auto characters_to_test = [] {
	using uchar_limits = std::numeric_limits<unsigned char>;
	constexpr auto min_uchar = uchar_limits::min();
	constexpr auto max_uchar = uchar_limits::max();
	std::array<int, static_cast<int>(max_uchar) - min_uchar + 1> vals = {};
	for (size_t i = 0; i + 1 < vals.size(); ++i)
		vals[i] = min_uchar + i;
	vals.back() = EOF;
	return vals;
}();

TEST(ctype, is_digit) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_digit(c), (bool)std::isdigit(c)) << " c = " << (int)c;
}

TEST(ctype, is_alpha) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_alpha(c), (bool)std::isalpha(c)) << " c = " << (int)c;
}

TEST(ctype, is_alnum) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_alnum(c), (bool)std::isalnum(c)) << " c = " << (int)c;
}

TEST(ctype, is_xdigit) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_xdigit(c), (bool)std::isxdigit(c)) << " c = " << (int)c;
}

TEST(ctype, is_lower) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_lower(c), (bool)std::islower(c)) << " c = " << (int)c;
}

TEST(ctype, is_upper) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_upper(c), (bool)std::isupper(c)) << " c = " << (int)c;
}

TEST(ctype, is_cntrl) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_cntrl(c), (bool)std::iscntrl(c)) << " c = " << (int)c;
}

TEST(ctype, is_space) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_space(c), (bool)std::isspace(c)) << " c = " << (int)c;
}

TEST(ctype, is_blank) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_blank(c), (bool)std::isblank(c)) << " c = " << (int)c;
}

TEST(ctype, is_graph) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_graph(c), (bool)std::isgraph(c)) << " c = " << (int)c;
}

TEST(ctype, is_print) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_print(c), (bool)std::isprint(c)) << " c = " << (int)c;
}

TEST(ctype, is_punct) {
	for (auto c : characters_to_test)
		EXPECT_EQ(is_punct(c), (bool)std::ispunct(c)) << " c = " << (int)c;
}

TEST(ctype, to_lower) {
	for (auto c : characters_to_test)
		EXPECT_EQ(to_lower(c), std::tolower(c)) << " c = " << (int)c;
}

TEST(ctype, to_upper) {
	for (auto c : characters_to_test)
		EXPECT_EQ(to_upper(c), std::toupper(c)) << " c = " << (int)c;
}
