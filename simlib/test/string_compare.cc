#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <simlib/ranges.hh>
#include <simlib/string_compare.hh>

using std::string;
using std::vector;

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, special_less) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, special_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, StrNumCompare) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(string_compare, StrVersionCompare) {
    vector<string> data = {
        // clang-format off
        "a000",
        "a001",
        "a00",
        "a01",
        "a010",
        "a09",
        "a090",
        "a0",
        "a1",
        "a9",
        "a10",
        "b000",
        "b001",
        "b00",
        "b01",
        "b010",
        "b09",
        "b090",
        "b0",
        "b1",
        "b9",
        "b10",
        // clang-format on
    };

    for (auto&& [i, sa] : enumerate_view(data)) {
        for (auto&& [j, sb] : enumerate_view(data)) {
            EXPECT_EQ(StrVersionCompare{}(sa, sb), i < j) << "sa: " << sa << " sb: " << sb;
        }
    }

    for (size_t i = 1; i < data.size(); ++i) {
        EXPECT_LT(strverscmp(data[i - 1].data(), data[i].data()), 0)
            << "i - 1: " << data[i - 1] << " i: " << data[i];
    }

    // If you think that this functions are equivalent...
    auto a = "a07xu";
    auto b = "a07291";
    EXPECT_EQ(StrVersionCompare{}(a, b), true);
    EXPECT_EQ((strverscmp(a, b) < 0), false);
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, SpecialStrCompare) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, LowerStrCompare) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, compare) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, compare_to) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, slow_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, is_digit_not_greater_than) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_string_compare, is_digit_not_less_than) {
    // TODO: implement it
}
