#include "simlib/utilities.hh"

#include <gtest/gtest.h>
#include <set>

using std::set;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, back_insert) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, binary_find) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, binary_find_with_comparator) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, binary_find_by) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, binary_find_by_with_comparator) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, binary_search) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, lower_bound) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, upper_bound) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, sort) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, sort_with_comparator) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, is_sorted) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, is_one_of) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_utilities, is_pair) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(utilities, filter) {
	set s = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	filter(s, [](int /*unused*/) { return true; });
	EXPECT_EQ(s, (set{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
	filter(s, [](int x) { return (x % 2 != 0); });
	EXPECT_EQ(s, (set{1, 3, 5, 7, 9}));
	filter(s, [](int x) { return (x % 5 != 0); });
	EXPECT_EQ(s, (set{1, 3, 7, 9}));
	filter(s, [](int x) { return (x % 5 != 0); });
	EXPECT_EQ(s, (set{1, 3, 7, 9}));
	filter(s, [](int x) { return (x % 3 != 0); });
	EXPECT_EQ(s, (set{1, 7}));
	filter(s, [](int /*unused*/) { return false; });
	EXPECT_EQ(s, (set<int>{}));
	filter(s, [](int /*unused*/) { return true; });
	EXPECT_EQ(s, (set<int>{}));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(utilities, filter_return_type) {
	set<int> s;
	// lvalue
	auto true_pred = [](int /*unused*/) { return true; };
	static_assert(std::is_same_v<set<int>&, decltype(filter(s, true_pred))>);
	// xvalue
	static_assert(
		std::is_same_v<set<int>, decltype(filter(set<int>{}, true_pred))>);
	// Check move only type
	struct Foo {
		Foo() = default;
		Foo(const Foo&) = delete;
		Foo(Foo&&) = default;
		Foo& operator=(const Foo&) = delete;
		Foo& operator=(Foo&&) = default;
		~Foo() = default;

		static int* begin() noexcept { return nullptr; }
		static int* end() noexcept { return nullptr; }
		void erase(int* /*unused*/) noexcept {}
	};
	(void)Foo(filter(Foo(), [](int /*unused*/) { return true; }));
}
