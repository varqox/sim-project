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

TEST(utilities, reverse_view_empty) {
	std::vector<int> a, b;
	for (int x : reverse_view(a))
		b.emplace_back(x);

	EXPECT_EQ(b, a);
}

TEST(utilities, reverse_view_lvalue) {
	std::vector a = {1, 2, 3, 4, 5};
	std::vector<int> b;
	for (int x : reverse_view(a))
		b.emplace_back(x);

	EXPECT_EQ(b, (std::vector {5, 4, 3, 2, 1}));

	std::vector<int> v {1};
	for (int x : reverse_view(v))
		EXPECT_NE(&v[0], &x);
	for (auto& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
	for (auto const& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
	for (auto&& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
}

TEST(utilities, reverse_view_double_lvalue) {
	std::vector<int> a {1, 2, 3, 4, 5};
	std::vector<int> b;
	for (int x : reverse_view(reverse_view(a)))
		b.emplace_back(x);

	EXPECT_EQ(b, (std::vector {1, 2, 3, 4, 5}));

	std::vector<int> v {1};
	for (int x : reverse_view(reverse_view(v)))
		EXPECT_NE(&v[0], &x);
	for (auto& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
	for (auto const& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
	for (auto&& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
}

TEST(utilities, reverse_view_simple_xvalue) {
	std::vector<int> b;
	for (int x : reverse_view(std::vector {1, 2, 3, 4, 5}))
		b.emplace_back(x);

	EXPECT_EQ(b, (std::vector {5, 4, 3, 2, 1}));
}

TEST(utilities, reverse_view_double_xvalue) {
	std::vector<int> b;
	for (int x : reverse_view(reverse_view(std::vector {1, 2, 3, 4, 5})))
		b.emplace_back(x);

	EXPECT_EQ(b, (std::vector {1, 2, 3, 4, 5}));
}

namespace {

struct inc_dead {
	int* p;
	~inc_dead() { ++*p; }
};

} // namespace

TEST(utilities, reverse_view_on_xvalue_lifetime) {
	int dead = 0;
	for (auto& x [[maybe_unused]] :
	     reverse_view(std::array {inc_dead {&dead}, inc_dead {&dead}})) {
		EXPECT_EQ(dead, 0);
	}
	EXPECT_EQ(dead, 2);
}

TEST(utilities, reverse_view_double_on_xvalue_lifetime) {
	int dead = 0;
	for (auto& x [[maybe_unused]] : reverse_view(
	        reverse_view(std::array {inc_dead {&dead}, inc_dead {&dead}}))) {
		EXPECT_EQ(dead, 0);
	}
	EXPECT_EQ(dead, 2);
}

TEST(utilities, reverse_view_and_sort) {
	std::vector v {4, 2, 5, 3, 1};
	auto rv = reverse_view(v);
	std::sort(rv.begin(), rv.end());
	EXPECT_EQ(v, (std::vector {5, 4, 3, 2, 1}));
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
