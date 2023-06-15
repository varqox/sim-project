#include <gtest/gtest.h>
#include <simlib/concat.hh>
#include <simlib/macros/throw.hh>

// NOLINTNEXTLINE
TEST(macros, THROW_MACRO) {
    try {
        THROW("a ", 1, -42, "c", '.', false, ";");
        ADD_FAILURE();
    } catch (const std::runtime_error& e) {
        constexpr auto line = __LINE__;
        EXPECT_EQ(
            e.what(),
            concat("a ", 1, -42, "c", '.', false, "; (thrown at ", __FILE__, ':', line - 3, ')')
        );
    } catch (...) {
        ADD_FAILURE();
    }
}
