#include "../include/ranges.hh"

#include <gtest/gtest.h>

using std::vector;

TEST(ranges, reverse_view_empty) {
	vector<int> a, b;
	for (int x : reverse_view(a))
		b.emplace_back(x);

	EXPECT_EQ(b, a);
}

TEST(ranges, reverse_view_lvalue) {
	vector a = {1, 2, 3, 4, 5};
	vector<int> b;
	for (int x : reverse_view(a))
		b.emplace_back(x);

	EXPECT_EQ(b, (vector {5, 4, 3, 2, 1}));

	vector<int> v {1};
	for (int x : reverse_view(v))
		EXPECT_NE(&v[0], &x);
	for (auto& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
	for (auto const& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
	for (auto&& x : reverse_view(v))
		EXPECT_EQ(&v[0], &x);
}

TEST(ranges, reverse_view_double_lvalue) {
	vector<int> a {1, 2, 3, 4, 5};
	vector<int> b;
	for (int x : reverse_view(reverse_view(a)))
		b.emplace_back(x);

	EXPECT_EQ(b, (vector {1, 2, 3, 4, 5}));

	vector<int> v {1};
	for (int x : reverse_view(reverse_view(v)))
		EXPECT_NE(&v[0], &x);
	for (auto& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
	for (auto const& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
	for (auto&& x : reverse_view(reverse_view(v)))
		EXPECT_EQ(&v[0], &x);
}

TEST(ranges, reverse_view_simple_xvalue) {
	vector<int> b;
	for (int x : reverse_view(vector {1, 2, 3, 4, 5}))
		b.emplace_back(x);

	EXPECT_EQ(b, (vector {5, 4, 3, 2, 1}));
}

TEST(ranges, reverse_view_double_xvalue) {
	vector<int> b;
	for (int x : reverse_view(reverse_view(vector {1, 2, 3, 4, 5})))
		b.emplace_back(x);

	EXPECT_EQ(b, (vector {1, 2, 3, 4, 5}));
}

namespace {

struct inc_dead {
	int* p;
	~inc_dead() { ++*p; }
};

} // namespace

TEST(ranges, reverse_view_on_xvalue_lifetime) {
	int dead = 0;
	for (auto& x [[maybe_unused]] :
	     reverse_view(std::array {inc_dead {&dead}, inc_dead {&dead}})) {
		EXPECT_EQ(dead, 0);
	}
	EXPECT_EQ(dead, 2);
}

TEST(ranges, reverse_view_double_on_xvalue_lifetime) {
	int dead = 0;
	for (auto& x [[maybe_unused]] : reverse_view(
	        reverse_view(std::array {inc_dead {&dead}, inc_dead {&dead}}))) {
		EXPECT_EQ(dead, 0);
	}
	EXPECT_EQ(dead, 2);
}

TEST(ranges, reverse_view_c_array) {
	int arr[] = {1, 2, 3};
	vector<int> res;
	for (int x : reverse_view(arr))
		res.emplace_back(x);
	EXPECT_EQ(res, (vector {3, 2, 1}));
}

TEST(ranges, reverse_view_and_sort) {
	vector v {4, 2, 5, 3, 1};
	auto rv = reverse_view(v);
	std::sort(rv.begin(), rv.end());
	EXPECT_EQ(v, (vector {5, 4, 3, 2, 1}));
}
