#include "simlib/avl_dict.hh"
#include "simlib/random.hh"

#include <gtest/gtest.h>
#include <set>
#include <type_traits>

template <class T, class U, class... Args>
void bulk_insert(T& container1, U& container2, Args&&... args) {
	(container1.emplace(args), ...);
	(container2.emplace(args), ...);
}

template <class T, decltype(*std::declval<T>().front(), 0) = 0>
auto elements(const T& avl_dict) {
	static_assert(not std::is_const_v<typename T::value_type>);
	std::vector<typename T::value_type> res;
	avl_dict.for_each([&](auto&& elem) { res.emplace_back(elem); });
	return res;
}

template <class T, decltype(std::declval<T>().begin(), 0) = 0>
auto elements(const T& stl_container) {
	static_assert(not std::is_const_v<typename T::value_type>);
	std::vector<typename T::value_type> res;
	res.reserve(stl_container.size());
	for (auto& elem : stl_container) {
		res.emplace_back(elem);
	}
	return res;
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(AvlDictContainer, foreach_since_lower_bound) {
	AVLDictMultiset<int> avl;
	std::multiset<int> rbt;
	bulk_insert(avl, rbt, 1, 2, 3, 3, 3, 5, 7, 9, 11, 11, 11, 13, 15);

	// foreach_since_lower_bound
	for (int beg = 0; beg < 22; ++beg) {
		for (int end = 0; end < 22; ++end) {
			std::vector<int> a;
			std::vector<int> r;
			avl.foreach_since_lower_bound(beg, [&](auto x) {
				if (x > end) {
					return false;
				}
				a.emplace_back(x);
				return true;
			});

			auto it = rbt.lower_bound(beg);
			while (it != rbt.end() and *it <= end) {
				r.emplace_back(*it++);
			}

			EXPECT_EQ(a, r) << "beg: " << beg << "end: " << end;
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(AvlDictContainer, foreach_since_upper_bound) {
	AVLDictMultiset<int> avl;
	std::multiset<int> rbt;
	bulk_insert(avl, rbt, 1, 2, 3, 3, 3, 5, 7, 9, 11, 11, 11, 13, 15);

	// foreach_since_upper_bound
	for (int beg = 0; beg < 22; ++beg) {
		for (int end = 0; end < 22; ++end) {
			std::vector<int> a;
			std::vector<int> r;
			avl.foreach_since_upper_bound(beg, [&](auto x) {
				if (x > end) {
					return false;
				}
				a.emplace_back(x);
				return true;
			});

			auto it = rbt.upper_bound(beg);
			while (it != rbt.end() and *it <= end) {
				r.emplace_back(*it++);
			}

			EXPECT_EQ(a, r) << "beg: " << beg << "end: " << end;
		}
	}
}

template <class T, class Cond>
static void set_filter(std::set<T>& set, Cond&& cond) {
	auto it = set.begin();
	while (it != set.end()) {
		if (cond(*it)) {
			it = set.erase(it);
		} else {
			++it;
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(AvlDictContainer, filter) {
	AVLDictSet<int> avl;
	std::set<int> rbt;
	bulk_insert(avl, rbt, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 21, 23,
	            25, 27, 29, 30, 32, 33, 34, 35, 37, 38, 39, 40);

	auto cond = [&](int x) { return bool(x & 1); };

	avl.filter(cond);
	set_filter(rbt, cond);
	EXPECT_EQ(elements(avl), elements(rbt));

	for (int iter = 0; iter < 1000; ++iter) {
		avl.clear();
		rbt.clear();

		int elems_num = get_random(0, 32);
		while (elems_num--) {
			bulk_insert(avl, rbt, get_random(0, 32));
		}

		avl.filter(cond);
		set_filter(rbt, cond);
		EXPECT_EQ(elements(avl), elements(rbt));
	}
}
