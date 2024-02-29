#include <gtest/gtest.h>
#include <simlib/slice.hh>
#include <vector>

using std::vector;

// NOLINTNEXTLINE
TEST(slice, default_constructor) {
    auto as = Slice<int>{};
    ASSERT_EQ(vector(as.begin(), as.end()), (vector<int>{}));
}

// NOLINTNEXTLINE
TEST(slice, constructs_from_literal) {
    [](auto as) {
        ASSERT_EQ(vector(as.begin(), as.end()), (vector{1, 2, 3}));
    }(Slice<int>{{1, 2, 3}});
}

// NOLINTNEXTLINE
TEST(slice, constructs_from_array) {
    int array[] = {5, 6};
    auto as = Slice<int>{array};
    ASSERT_EQ(vector(as.begin(), as.end()), (vector{5, 6}));
}

// NOLINTNEXTLINE
TEST(slice, constructs_from_data_and_size) {
    int x = 42;
    auto as = Slice<int>{&x, 1};
    ASSERT_EQ(vector(as.begin(), as.end()), (vector{x}));
}

// NOLINTNEXTLINE
TEST(slice, constructs_from_vector) {
    auto vec = vector{1, 2, 3, 4, 5};
    auto as = Slice<int>{vec};
    ASSERT_EQ(vector(as.begin(), as.end()), (vector{1, 2, 3, 4, 5}));
}

// NOLINTNEXTLINE
TEST(slice, size) {
    ASSERT_EQ((Slice<int>{{1, 2, 3}}.size()), 3);
    ASSERT_EQ((Slice<int>{vector<int>{}}.size()), 0);
}

// NOLINTNEXTLINE
TEST(slice, is_empty) {
    ASSERT_EQ((Slice<int>{}.is_empty()), true);
    ASSERT_EQ((Slice<int>{{1}}.is_empty()), false);
}

// NOLINTNEXTLINE
TEST(slice, data) {
    ASSERT_EQ((Slice<int>{{1, 2, 3}}.data()[0]), 1);
    ASSERT_EQ((Slice<int>{{1, 2, 3}}.data()[1]), 2);
    ASSERT_EQ((Slice<int>{{1, 2, 3}}.data()[2]), 3);
}

// NOLINTNEXTLINE
TEST(slice, operator_subscript) {
    ASSERT_EQ(Slice<int>({1, 2, 3})[0], 1);
    ASSERT_EQ(Slice<int>({1, 2, 3})[1], 2);
    ASSERT_EQ(Slice<int>({1, 2, 3})[2], 3);
}
