#include "../include/utilities.hh"

#include <gtest/gtest.h>
#include <set>

TEST(DISABLED_utilities, back_insert) {
	// TODO: implement it
}

TEST(DISABLED_utilities, binary_find) {
	// TODO: implement it
}

TEST(DISABLED_utilities, binary_find_with_comparator) {
	// TODO: implement it
}

TEST(DISABLED_utilities, binary_find_by) {
	// TODO: implement it
}

TEST(DISABLED_utilities, binary_find_by_with_comparator) {
	// TODO: implement it
}

TEST(DISABLED_utilities, binary_search) {
	// TODO: implement it
}

TEST(DISABLED_utilities, lower_bound) {
	// TODO: implement it
}

TEST(DISABLED_utilities, upper_bound) {
	// TODO: implement it
}

TEST(DISABLED_utilities, sort) {
	// TODO: implement it
}

TEST(DISABLED_utilities, sort_with_comparator) {
	// TODO: implement it
}

TEST(DISABLED_utilities, is_sorted) {
	// TODO: implement it
}

TEST(DISABLED_utilities, is_one_of) {
	// TODO: implement it
}

TEST(DISABLED_utilities, is_pair) {
	// TODO: implement it
}

TEST(utilities, filter) {
	std::set s = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	filter(s, [](int) { return true; });
	EXPECT_EQ(s, (std::set {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
	filter(s, [](int x) { return (x % 2 != 0); });
	EXPECT_EQ(s, (std::set {1, 3, 5, 7, 9}));
	filter(s, [](int x) { return (x % 5 != 0); });
	EXPECT_EQ(s, (std::set {1, 3, 7, 9}));
	filter(s, [](int x) { return (x % 5 != 0); });
	EXPECT_EQ(s, (std::set {1, 3, 7, 9}));
	filter(s, [](int x) { return (x % 3 != 0); });
	EXPECT_EQ(s, (std::set {1, 7}));
	filter(s, [](int) { return false; });
	EXPECT_EQ(s, (std::set<int> {}));
	filter(s, [](int) { return true; });
	EXPECT_EQ(s, (std::set<int> {}));
}
