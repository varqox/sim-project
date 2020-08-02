#include "simlib/string_view.hh"
#include "simlib/to_string.hh"

#include <array>
#include <gtest/gtest.h>
#include <type_traits>

using std::array; // NOLINT(misc-unused-using-decls)
using std::string;

template <class T, class U>
void check_str(T&& str, U&& data, size_t size) {
	EXPECT_EQ(str.data(), data);
	EXPECT_EQ(str.size(), size);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, default_constructor) {
	EXPECT_EQ(StringBase<char>{}.size(), 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_nullptr) {
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, std::nullptr_t>));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_const_char_array) {
	const char str[] = "abc";
	check_str(StringBase<const char>{str}, str, 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_char_array) {
	char str[] = "abc";
	check_str(StringBase<char>{str}, str, 3);
	check_str(StringBase<const char>{str}, str, 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_const_StaticCStringBuff) {
	StaticCStringBuff str{"abc"};
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, decltype(str)>));
	check_str(StringBase<const char>{str}, str.data(), str.size());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_ptr) {
	array str{'a', '\0', 'b', '\0'};
	check_str(StringBase<char>{str.data()}, str.data(), 1);
	check_str(StringBase<const char>{str.data()}, str.data(), 1);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_ptr_with_size) {
	array str{'a', '\0', 'b', '\0'};
	check_str(StringBase<char>{str.data(), 4}, str.data(), 4);
	check_str(StringBase<const char>{str.data(), 4}, str.data(), 4);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_temporary_string) {
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, string>));
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, string&&>));
	EXPECT_FALSE((std::is_constructible_v<StringBase<const char>, string>));
	EXPECT_FALSE((std::is_constructible_v<StringBase<const char>, string&&>));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_temporary_const_string) {
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, const string>));
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, const string&&>));
	EXPECT_FALSE(
	   (std::is_constructible_v<StringBase<const char>, const string>));
	EXPECT_FALSE(
	   (std::is_constructible_v<StringBase<const char>, const string&&>));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_const_string_ref) {
	const string str = "abcdef";
	check_str(StringBase<const char>{str}, str.data(), str.size());
	check_str(StringBase<const char>{str, 2}, str.data() + 2, str.size() - 2);
	check_str(StringBase<const char>{str, 2, 3}, str.data() + 2, 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, constructor_from_string_ref) {
	string str = "abcdef";
	check_str(StringBase<char>{str}, str.data(), str.size());
	check_str(StringBase<char>{str, 2}, str.data() + 2, str.size() - 2);
	check_str(StringBase<char>{str, 2, 3}, str.data() + 2, 3);
	check_str(StringBase<const char>{str}, str.data(), str.size());
	check_str(StringBase<const char>{str, 2}, str.data() + 2, str.size() - 2);
	check_str(StringBase<const char>{str, 2, 3}, str.data() + 2, 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, copy_constructor) {
	char str[] = "abc";
	{
		StringBase<char> s(str);
		auto x = s;
		check_str(x, str, 3);
	}
	{
		StringBase<const char> s(str);
		auto x = s;
		check_str(x, str, 3);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, move_constructor) {
	char str[] = "abc";
	{
		StringBase<char> s(str);
		auto x = std::move(s); // NOLINT
		check_str(x, str, 3);
	}
	{
		StringBase<const char> s(str);
		auto x = std::move(s); // NOLINT
		check_str(x, str, 3);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, copy_assignment) {
	char str[] = "abc";
	{
		StringBase<char> s(str);
		StringBase<char> x;
		x = s;
		check_str(x, str, 3);
	}
	{
		StringBase<const char> s(str);
		StringBase<const char> x;
		x = s;
		check_str(x, str, 3);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, move_assignment) {
	char str[] = "abc";
	{
		StringBase<char> s(str);
		StringBase<char> x;
		x = std::move(s); // NOLINT
		check_str(x, str, 3);
	}
	{
		StringBase<const char> s(str);
		StringBase<const char> x;
		x = std::move(s); // NOLINT
		check_str(x, str, 3);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, assignment_from_nullptr) {
	EXPECT_FALSE((std::is_assignable_v<StringBase<char>, std::nullptr_t>));
	EXPECT_FALSE(
	   (std::is_assignable_v<StringBase<const char>, std::nullptr_t>));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, empty) {
	EXPECT_EQ(StringBase<const char>().empty(), true);
	EXPECT_EQ(StringBase<const char>("").empty(), true);
	EXPECT_EQ(StringBase<const char>("a").empty(), false);
	EXPECT_EQ(StringBase<const char>("ab").empty(), false);
	EXPECT_EQ(StringBase<const char>("abc").empty(), false);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, size) {
	EXPECT_EQ(StringBase<const char>().size(), 0);
	EXPECT_EQ(StringBase<const char>("").size(), 0);
	EXPECT_EQ(StringBase<const char>("a").size(), 1);
	EXPECT_EQ(StringBase<const char>("ab").size(), 2);
	EXPECT_EQ(StringBase<const char>("abc").size(), 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, length) {
	EXPECT_EQ(StringBase<const char>().length(), 0);
	EXPECT_EQ(StringBase<const char>("").length(), 0);
	EXPECT_EQ(StringBase<const char>("a").length(), 1);
	EXPECT_EQ(StringBase<const char>("ab").length(), 2);
	EXPECT_EQ(StringBase<const char>("abc").length(), 3);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringBase, data) {
	char str[] = "abcd";
	{
		StringBase<const char> s = str;
		EXPECT_TRUE((std::is_same_v<const char*, decltype(s.data())>));
		EXPECT_EQ(s.data(), str);
	}
	{
		const StringBase<const char> s = str;
		EXPECT_TRUE((std::is_same_v<const char*, decltype(s.data())>));
		EXPECT_EQ(s.data(), str);
	}
	{
		StringBase<char> s = str;
		EXPECT_TRUE((std::is_same_v<char*, decltype(s.data())>));
		EXPECT_EQ(s.data(), str);
	}
	{
		const StringBase<char> s = str;
		EXPECT_TRUE((std::is_same_v<const char*, decltype(s.data())>));
		EXPECT_EQ(s.data(), str);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, begin) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, cbegin) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, cend) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rbegin) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rend) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, crbegin) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, crend) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, front) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, back) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, operator_subscript) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, at) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, compare) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_beg_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_beg_and_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_beg_and_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_beg_and_end_and_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_beg_and_end_and_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase,
     find_with_beg_and_end_and_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_char) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_char_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, find_with_char_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_beg_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_beg_and_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_beg_and_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_beg_and_end_and_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_beg_and_end_and_StringBase_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase,
     rfind_with_beg_and_end_and_StringBase_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_char) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_char_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, rfind_with_char_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringBase, to_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, string_operator_add_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, basic_ostream_operator_left_bitshift_StringBase) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringView, default_constructor) { EXPECT_EQ(StringView{}.size(), 0); }

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(StringView, constructor_from_nullptr) {
	EXPECT_FALSE((std::is_constructible_v<StringBase<char>, std::nullptr_t>));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_const_char_array) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_char_array) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_const_StaticCStringBuff) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_const_char_ptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_const_char_ptr_with_size) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_temporary_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_temporary_const_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_const_string_ref) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, copy_constructor) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, move_constructor) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, copy_assignment) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, move_assignment) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, assignment_from_nullptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, constructor_from_temporary) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_prefix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_suffix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, extract_prefix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, extract_suffix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_leading_with_func) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_leading_with_char) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_trailing_with_func) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, remove_trailing_with_char) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, extract_leading) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, extract_trailing) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, without_prefix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, without_suffix) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, without_leading) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, without_trailing) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, substr_with_pos) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, substr_with_pos_and_count) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, substring_with_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_StringView, substring_with_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, string_operator_add_StringView) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, intentional_unsafe_string_view_from_const_char_ptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, intentional_unsafe_string_view_from_StringView) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view,
     intentional_unsafe_string_view_from_const_string_ref) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_not_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_not_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, operator_not_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, default_constructor) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_nullptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_const_char_array) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_char_array) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_const_StaticCStringBuff) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_temporary_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_temporary_const_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_const_string_ref) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_const_char_ptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_ptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_const_char_ptr_with_size) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, copy_constructor) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, move_constructor) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, copy_assignment) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, move_assignment) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, assignment_from_nullptr) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, constructor_from_temporary) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, operator_StringView) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, substr_with_pos) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, substr_with_pos_and_count) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, substring_with_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, substring_with_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_CStringView, c_str) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, find_with_StringView_and_char) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, find_with_StringView_and_char_and_beg) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, find_with_StringView_and_char_and_beg_and_end) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, remove_trailing_with_func) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_string_view, remove_trailing_with_char) {
	// TODO: implement it
}
