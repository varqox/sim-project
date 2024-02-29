#include <gtest/gtest.h>
#include <simlib/to_arg_seq.hh>
#include <string_view>
#include <vector>

using std::string_view;
using std::vector;

// NOLINTNEXTLINE
TEST(to_arg_seq, argc_0) {
    char* argv[] = {nullptr};
    auto arg_seq = to_arg_seq(0, argv);
    ASSERT_EQ(arg_seq.begin(), arg_seq.end());
}

// NOLINTNEXTLINE
TEST(to_arg_seq, argc_1) {
    char prog[] = "prog";
    char* argv[] = {prog, nullptr};
    auto arg_seq = to_arg_seq(1, argv);
    ASSERT_EQ(arg_seq.begin(), arg_seq.end());
}

// NOLINTNEXTLINE
TEST(to_arg_seq, argc_2) {
    char prog[] = "prog";
    char arg1[] = "arg1";
    char* argv[] = {prog, arg1, nullptr};
    auto arg_seq = to_arg_seq(2, argv);
    auto args = vector(arg_seq.begin(), arg_seq.end());
    ASSERT_EQ(args, (vector<string_view>{"arg1"}));
}

// NOLINTNEXTLINE
TEST(to_arg_seq, argc_3) {
    char prog[] = "prog";
    char arg1[] = "arg1";
    char arg2[] = "arg2";
    char* argv[] = {prog, arg1, arg2, nullptr};
    auto arg_seq = to_arg_seq(3, argv);
    auto args = vector(arg_seq.begin(), arg_seq.end());
    ASSERT_EQ(args, (vector<string_view>{"arg1", "arg2"}));
}

// NOLINTNEXTLINE
TEST(to_arg_seq, argc_3_not_full_argv) {
    char prog[] = "prog";
    char arg1[] = "arg1";
    char arg2[] = "arg2";
    char* argv[] = {prog, arg1, arg2, nullptr};
    auto arg_seq = to_arg_seq(2, argv);
    auto args = vector(arg_seq.begin(), arg_seq.end());
    ASSERT_EQ(args, (vector<string_view>{"arg1"}));

    arg_seq = to_arg_seq(1, argv);
    args = vector(arg_seq.begin(), arg_seq.end());
    ASSERT_EQ(args, (vector<string_view>{}));

    arg_seq = to_arg_seq(0, argv);
    args = vector(arg_seq.begin(), arg_seq.end());
    ASSERT_EQ(args, (vector<string_view>{}));
}
