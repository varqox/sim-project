#include <gtest/gtest.h>
#include <simlib/err_defer.hh>
#include <stdexcept>

// NOLINTNEXTLINE
TEST(DISABLED_ErrDefer, no_exception) {
    int run_cnt = 0;
    {
        ErrDefer guard = [&] { ++run_cnt; };
    }
    [&] { ErrDefer guard = [&] { ++run_cnt; }; }();
    EXPECT_EQ(run_cnt, 0);
}

// NOLINTNEXTLINE
TEST(DISABLED_ErrDefer, exception_simple) {
    int run_cnt = 0;
    try {
        ErrDefer guard = [&] { ++run_cnt; };
        throw std::runtime_error{""};
    } catch (...) {
    }
    EXPECT_EQ(run_cnt, 1);
}

// NOLINTNEXTLINE
TEST(DISABLED_ErrDefer, in_catch) {
    int run_cnt = 0;
    try {
        throw std::runtime_error{""};
    } catch (...) {
        ErrDefer guard = [&] { ++run_cnt; };
    }
    EXPECT_EQ(run_cnt, 0);
}

// NOLINTNEXTLINE
TEST(DISABLED_ErrDefer, exception_in_catch) {
    int run_cnt = 0;
    try {
        throw std::runtime_error{""};
    } catch (...) {
        try {
            ErrDefer guard = [&] { ++run_cnt; };
            throw std::runtime_error{""};
        } catch (...) {
        }
    }
    EXPECT_EQ(run_cnt, 1);
}

// NOLINTNEXTLINE
TEST(DISABLED_ErrDefer, in_catch_in_catch) {
    int run_cnt = 0;
    try {
        throw std::runtime_error{""};
    } catch (...) {
        try {
            throw std::runtime_error{""};
        } catch (...) {
            ErrDefer guard = [&] { ++run_cnt; };
        }
    }
    EXPECT_EQ(run_cnt, 0);
}
