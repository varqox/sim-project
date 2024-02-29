#include <gtest/gtest.h>
#include <simlib/macros/macros.hh>
#include <simlib/macros/stringify.hh>
#include <simlib/string_view.hh>

#define XXX() x x x
#define YYY(...) y y y __VA_ARGS__

// NOLINTNEXTLINE
TEST(macros, PRIMITIVE_STRINGIFY) {
    static_assert(StringView{PRIMITIVE_STRINGIFY(XXX())} == "XXX()");
    static_assert(StringView{PRIMITIVE_STRINGIFY(XXX(), XXX(), XXX())} == "XXX(), XXX(), XXX()");
    static_assert(StringView{PRIMITIVE_STRINGIFY()}.empty());
}

// NOLINTNEXTLINE
TEST(macros, STRINGIFY) {
    static_assert(StringView{STRINGIFY(XXX())} == "x x x");
    static_assert(StringView{STRINGIFY(DEFER1(XXX)())} == "XXX ()");
    static_assert(StringView{STRINGIFY(XXX(), XXX(), XXX())} == "x x x, x x x, x x x");
    static_assert(StringView{STRINGIFY()}.empty());
}
