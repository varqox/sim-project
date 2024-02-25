#include <gtest/gtest.h>
#include <simlib/concat.hh>
#include <simlib/throw_assert.hh>

// NOLINTNEXTLINE
TEST(throw_assert, throw_assert) {
    constexpr int var = 123;
    try {
        throw_assert(var == 123);
        throw_assert(var % 3 != 0);
        ADD_FAILURE();
    } catch (const std::runtime_error& e) {
        constexpr auto line = __LINE__;
        EXPECT_EQ(
            e.what(),
            concat(
                __FILE__,
                ':',
                line - 3,
                ": ",
                __PRETTY_FUNCTION__,
                ": Assertion `var % 3 != 0` failed."
            )
        );
    } catch (...) {
        ADD_FAILURE();
    }
}
