#include "../include/avl_dict.hh"

#include <gtest/gtest.h>
#include <set>

TEST(AvlDictContainer, foreach_since_lower_bound) {
	AVLDictMultiset<int> avl;
	std::multiset<int> rbt;

	auto insert = [&](int x) {
		avl.emplace(x);
		rbt.emplace(x);
	};

	insert(1);
	insert(2);
	insert(3);
	insert(3);
	insert(3);
	insert(5);
	insert(7);
	insert(9);
	insert(11);
	insert(11);
	insert(11);
	insert(13);
	insert(15);

	// foreach_since_lower_bound
	for (int beg = 0; beg < 22; ++beg)
		for (int end = 0; end < 22; ++end) {
			std::vector<int> a, r;
			avl.foreach_since_lower_bound(beg, [&](auto x) {
				if (x > end)
					return false;
				a.emplace_back(x);
				return true;
			});

			auto it = rbt.lower_bound(beg);
			while (it != rbt.end() and *it <= end)
				r.emplace_back(*it++);

			EXPECT_EQ(a, r) << "beg: " << beg << "end: " << end;
		}
}

TEST(AvlDictContainer, foreach_since_upper_bound) {
	AVLDictMultiset<int> avl;
	std::multiset<int> rbt;

	auto insert = [&](int x) {
		avl.emplace(x);
		rbt.emplace(x);
	};

	insert(1);
	insert(2);
	insert(3);
	insert(3);
	insert(3);
	insert(5);
	insert(7);
	insert(9);
	insert(11);
	insert(11);
	insert(11);
	insert(13);
	insert(15);

	// foreach_since_upper_bound
	for (int beg = 0; beg < 22; ++beg)
		for (int end = 0; end < 22; ++end) {
			std::vector<int> a, r;
			avl.foreach_since_upper_bound(beg, [&](auto x) {
				if (x > end)
					return false;
				a.emplace_back(x);
				return true;
			});

			auto it = rbt.upper_bound(beg);
			while (it != rbt.end() and *it <= end)
				r.emplace_back(*it++);

			EXPECT_EQ(a, r) << "beg: " << beg << "end: " << end;
		}
}
