#include <gtest/gtest.h>
#include <initializer_list>
#include <set>
#include <simlib/merge.hh>
#include <vector>

using std::initializer_list;
using std::set;
using std::vector;

// NOLINTNEXTLINE
TEST(merge, vector_vector) {
    ASSERT_EQ(merge(vector{1, 2}, vector<int>{}), (vector{1, 2}));
    ASSERT_EQ(merge(vector{1, 2}, vector{3, 4, 5}), (vector{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(merge, vector_initializer_list) {
    ASSERT_EQ(merge(vector{1, 2}, initializer_list<int>{}), (vector{1, 2}));
    ASSERT_EQ(merge(vector{1, 2}, initializer_list<int>{3, 4, 5}), (vector{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(merge, vector_set) {
    ASSERT_EQ(merge(vector{1, 2}, set<int>{}), (vector{1, 2}));
    ASSERT_EQ(merge(vector{1, 2}, set<int>{5, 3, 4}), (vector{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(merge, set_vector) {
    ASSERT_EQ(merge(set{1, 2}, vector<int>{}), (set{1, 2}));
    ASSERT_EQ(merge(set{1, 2}, vector{2, 3, 4, 5}), (set{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(merge, set_initializer_list) {
    ASSERT_EQ(merge(set{1, 2}, initializer_list<int>{}), (set{1, 2}));
    ASSERT_EQ(merge(set{1, 2}, initializer_list<int>{2, 3, 4, 5}), (set{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(merge, set_set) {
    ASSERT_EQ(merge(set{1, 2}, set<int>{}), (set{1, 2}));
    ASSERT_EQ(merge(set{1, 2}, set<int>{5, 2, 3, 4}), (set{1, 2, 3, 4, 5}));
}
