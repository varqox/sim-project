#include "simlib/macros.hh"
#include "simlib/string_view.hh"

#include <gtest/gtest.h>

#define ABC a b c
#define XYZ x y z
#define KLM k l m

#define XXX() x x x
#define YYY(...) y y y __VA_ARGS__

// NOLINTNEXTLINE
TEST(EnumVal, PRIMITIVE_STRINGIFY) {
    static_assert(StringView{PRIMITIVE_STRINGIFY(XXX())} == "XXX()");
    static_assert(
            StringView{PRIMITIVE_STRINGIFY(XXX(), XXX(), XXX())} == "XXX(), XXX(), XXX()");
    static_assert(StringView{PRIMITIVE_STRINGIFY()} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, STRINGIFY) {
    static_assert(StringView{STRINGIFY(XXX())} == "x x x");
    static_assert(StringView{STRINGIFY(DEFER1(XXX)())} == "XXX ()");
    static_assert(StringView{STRINGIFY(XXX(), XXX(), XXX())} == "x x x, x x x, x x x");
    static_assert(StringView{STRINGIFY()} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, EAT) {
    static_assert(StringView{STRINGIFY(EAT(CAT((), ())))} == "");
    static_assert(StringView{STRINGIFY(EAT(abc, d, ef))} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, PRIMITIVE_CAT) {
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT(ABC, XYZ))} == "ABCXYZ");
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT(ABC, XYZ, ABC, XYZ))} ==
            "ABCXYZ, a b c, x y z");
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT(ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT(ABC, ))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT(, ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_CAT())} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, CAT) {
    static_assert(StringView{STRINGIFY(CAT(ABC, XYZ))} == "a b cx y z");
    static_assert(StringView{STRINGIFY(CAT(kk, XYZ))} == "kkx y z");
    static_assert(
            StringView{STRINGIFY(CAT(ABC, XYZ, ABC, XYZ))} == "a b cx y z, a b c, x y z");
    static_assert(StringView{STRINGIFY(CAT(ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(CAT(ABC, ))} == "a b c");
    static_assert(StringView{STRINGIFY(CAT(, ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(CAT())} == "");
    static_assert(StringView{STRINGIFY(CAT(XYZ, DEFER1(EAT)()))} == "x y zEAT ()");
}

// NOLINTNEXTLINE
TEST(EnumVal, PRIMITIVE_REV_CAT) {
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT(ABC, XYZ))} == "XYZABC");
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT(ABC, XYZ, ABC, XYZ))} ==
            "x y z, a b c, XYZABC");
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT(ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT(ABC, ))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT(, ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(PRIMITIVE_REV_CAT())} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, REV_CAT) {
    static_assert(StringView{STRINGIFY(REV_CAT(ABC, XYZ))} == "x y za b c");
    static_assert(StringView{STRINGIFY(REV_CAT(kk, XYZ))} == "x y zkk");
    static_assert(
            StringView{STRINGIFY(REV_CAT(ABC, XYZ, ABC, XYZ))} == "x y z, a b c, x y za b c");
    static_assert(StringView{STRINGIFY(REV_CAT(ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(REV_CAT(ABC, ))} == "a b c");
    static_assert(StringView{STRINGIFY(REV_CAT(, ABC))} == "a b c");
    static_assert(StringView{STRINGIFY(REV_CAT())} == "");
    static_assert(StringView{STRINGIFY(REV_CAT(DEFER1(EAT)(), XYZ))} == "x y zEAT ()");
}

// NOLINTNEXTLINE
TEST(EnumVal, PRIMITIVE_DOUBLE_CAT) {
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(ABC, XYZ, KLM))} == "ABCXYZKLM");
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(ABC, XYZ, KLM, ABC, XYZ, KLM))} ==
            "ABCXYZKLM, a b c, x y z, k l m");
#define ABCXYZ abcxyz
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(ABC, XYZ))} == "abcxyz");
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(ABC, XYZ, ))} == "abcxyz");
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(, ABC, XYZ))} == "abcxyz");
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(ABC, , XYZ))} == "abcxyz");
    static_assert(StringView{STRINGIFY(PRIMITIVE_DOUBLE_CAT(, ))} == "");
#undef ABCXYZ
}

// NOLINTNEXTLINE
TEST(EnumVal, EMPTY) {
    static_assert(StringView{STRINGIFY(EMPTY)} == "EMPTY");
    static_assert(StringView{STRINGIFY(EMPTY())} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, EXPAND) {
    static_assert(StringView{STRINGIFY(EXPAND())} == "");
    static_assert(StringView{STRINGIFY(EXPAND(XXX()))} == "x x x");
    static_assert(StringView{STRINGIFY(EXPAND(XXX EMPTY()()))} == "x x x");
    static_assert(StringView{STRINGIFY(EXPAND(XXX EMPTY EMPTY()()()))} == "XXX ()");
    static_assert(StringView{STRINGIFY(EXPAND(XXX(), XXX()))} == "x x x, x x x");
    static_assert(
            StringView{STRINGIFY(EXPAND(XXX EMPTY()(), XXX EMPTY()()))} == "x x x, x x x");
    static_assert(StringView{STRINGIFY(EXPAND(
                          XXX EMPTY EMPTY()()(), XXX EMPTY EMPTY()()()))} == "XXX (), XXX ()");
}

// NOLINTNEXTLINE
TEST(EnumVal, DEFER1) {
    static_assert(StringView{STRINGIFY(DEFER1())} == "");
    static_assert(StringView{STRINGIFY(DEFER1(YYY)(a, b))} == "YYY (a, b)");
    static_assert(StringView{STRINGIFY(EXPAND(DEFER1(YYY)(a, b)))} == "y y y a, b");
}

// NOLINTNEXTLINE
TEST(EnumVal, DEFER2) {
    static_assert(StringView{STRINGIFY(EXPAND(DEFER2()))} == "");
    static_assert(StringView{STRINGIFY(EXPAND(DEFER2(YYY)(a, b)))} == "YYY (a, b)");
    static_assert(StringView{STRINGIFY(EXPAND(EXPAND(DEFER2(YYY)(a, b))))} == "y y y a, b");
}

// NOLINTNEXTLINE
TEST(EnumVal, DEFER3) {
    static_assert(StringView{STRINGIFY(EXPAND(EXPAND(DEFER3())))} == "");
    static_assert(StringView{STRINGIFY(EXPAND(EXPAND(DEFER3(YYY)(a, b))))} == "YYY (a, b)");
    static_assert(
            StringView{STRINGIFY(EXPAND(EXPAND(EXPAND(DEFER3(YYY)(a, b)))))} == "y y y a, b");
}

// NOLINTNEXTLINE
TEST(EnumVal, LPAREN_RPAREN) {
    static_assert(StringView{STRINGIFY(LPAREN)} == "LPAREN");
    static_assert(StringView{STRINGIFY(RPAREN)} == "RPAREN");
    static_assert(StringView{STRINGIFY(LPAREN() RPAREN())} == "( )");
    static_assert(StringView{STRINGIFY(YYY LPAREN() a, b RPAREN())} == "YYY ( a, b )");
    static_assert(StringView{STRINGIFY(EXPAND(YYY LPAREN() a, b RPAREN()))} == "y y y a, b");
}

// NOLINTNEXTLINE
TEST(EnumVal, COMMA) {
    static_assert(StringView{STRINGIFY(COMMA)} == "COMMA");
    static_assert(StringView{STRINGIFY(COMMA())} == ",");
    static_assert(StringView{STRINGIFY(CAT(a COMMA() b, c))} == "ab, c");
    static_assert(StringView{STRINGIFY(CAT(a DEFER1(COMMA)() b, c))} == "a , bc");
}

// NOLINTNEXTLINE
TEST(EnumVal, FOLDR) {
    static_assert(StringView{STRINGIFY(FOLDR(XX, (1)(2)(3, 4)(5)()(6), 42))} ==
            "XX((1), XX((2), XX((3, 4), XX((5), XX((), XX((6), 42))))))");
    static_assert(StringView{STRINGIFY(FOLDR(XX, , 42))} == "42");

#define FOLDR_TESTER(args, ...) args, (__VA_ARGS__)
    static_assert(StringView{STRINGIFY(FOLDR(FOLDR_TESTER, (1)(2)(3, 4)(5)()(6), 42))} ==
            "(1), ((2), ((3, 4), ((5), ((), ((6), (42))))))");
    static_assert(StringView{STRINGIFY(FOLDR(FOLDR_TESTER, , 42))} == "42");
#undef FOLDR_TESTER

#define FOLDR_TESTER2(args, acc1, acc2, ...) acc2, acc1, args + __VA_ARGS__
    static_assert(StringView{STRINGIFY(FOLDR(FOLDR_TESTER2, , xx, yy, ))} == "xx, yy,");
    static_assert(
            StringView{STRINGIFY(FOLDR(FOLDR_TESTER2, (1), xx, yy, ))} == "yy, xx, (1) +");
    static_assert(StringView{STRINGIFY(FOLDR(FOLDR_TESTER2, (1)(2)(3, 4)(5)()(6), xx,
                          yy, ))} == "xx, yy, (1) + (2) + (3, 4) + (5) + () + (6) +");
    static_assert(StringView{STRINGIFY(FOLDR(FOLDR_TESTER2, (1)(2)(3, 4)(5)()(6)(), xx,
                          yy, ))} == "yy, xx, (1) + (2) + (3, 4) + (5) + () + (6) + () +");
#undef FOLDR_TESTER
}

// NOLINTNEXTLINE
TEST(EnumVal, MAP) {
    static_assert(StringView{STRINGIFY(MAP(XX, (1)(2, 3)(4)))} == "XX (1) XX (2, 3) XX (4)");
    static_assert(StringView{STRINGIFY(MAP(XX, (1)(2)(3, 4)(5)()(6)))} ==
            "XX (1) XX (2) XX (3, 4) XX (5) XX () XX (6)");
    static_assert(StringView{STRINGIFY(MAP(XX, ))} == "");

    static_assert(StringView{STRINGIFY(MAP(STRINGIFY, (1)(2)(3, 4)(5)()(6)))} ==
            R"("1" "2" "3, 4" "5" "" "6")");
    static_assert(StringView{STRINGIFY(MAP(STRINGIFY, ))} == "");
}

// NOLINTNEXTLINE
TEST(EnumVal, MAP_DELIM) {
    static_assert(StringView{STRINGIFY(MAP_DELIM(XX, delim, (1)(2, 3)(4)))} ==
            "XX (1) delim XX (2, 3) delim XX (4)");
    static_assert(StringView{STRINGIFY(MAP_DELIM(XX, aa, ))} == "");
    static_assert(StringView{STRINGIFY(MAP_DELIM(XX, aa, (1)))} == "XX (1)");
    static_assert(StringView{STRINGIFY(MAP_DELIM(XX, aa, (1)(2)))} == "XX (1) aa XX (2)");
    static_assert(StringView{STRINGIFY(MAP_DELIM(XX, aa, (1)(2)(3, 4)))} ==
            "XX (1) aa XX (2) aa XX (3, 4)");
    static_assert(StringView{STRINGIFY(MAP_DELIM(YYY, ||, ))} == "");
    static_assert(StringView{STRINGIFY(MAP_DELIM(YYY, ||, (1)))} == "y y y 1");
    static_assert(StringView{STRINGIFY(MAP_DELIM(YYY, ||, (1)(2)))} == "y y y 1 || y y y 2");
    static_assert(StringView{STRINGIFY(MAP_DELIM(YYY, ||, (1)(2)(3, 4)))} ==
            "y y y 1 || y y y 2 || y y y 3, 4");
    static_assert(StringView{STRINGIFY(MAP_DELIM(YYY, ||, (1)(2)(3, 4)(5)()(6)))} ==
            "y y y 1 || y y y 2 || y y y 3, 4 || y y y 5 || y y y || y y y 6");
    static_assert(StringView{STRINGIFY(MAP_DELIM(STRINGIFY, ++, (1)(2)(3, 4)(5)()(6)))} ==
            R"("1" ++ "2" ++ "3, 4" ++ "5" ++ "" ++ "6")");
}

// NOLINTNEXTLINE
TEST(EnumVal, MAP_DELIM_FUNC) {
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(XX, delim, (1)(2, 3)(4)))} ==
            "XX (1) delim() XX (2, 3) delim() XX (4)");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(XX, COMMA, ))} == "");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(XX, COMMA, (1)))} == "XX (1)");
    static_assert(
            StringView{STRINGIFY(MAP_DELIM_FUNC(XX, COMMA, (1)(2)))} == "XX (1) , XX (2)");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(XX, COMMA, (1)(2)(3, 4)))} ==
            "XX (1) , XX (2) , XX (3, 4)");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(YYY, COMMA, ))} == "");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(YYY, COMMA, (1)))} == "y y y 1");
    static_assert(
            StringView{STRINGIFY(MAP_DELIM_FUNC(YYY, COMMA, (1)(2)))} == "y y y 1 , y y y 2");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(YYY, COMMA, (1)(2)(3, 4)))} ==
            "y y y 1 , y y y 2 , y y y 3, 4");
    static_assert(StringView{STRINGIFY(MAP_DELIM_FUNC(YYY, COMMA, (1)(2)(3, 4)(5)()(6)))} ==
            "y y y 1 , y y y 2 , y y y 3, 4 , y y y 5 , y y y , y y y 6");
    static_assert(
            StringView{STRINGIFY(MAP_DELIM_FUNC(STRINGIFY, XXX, (1)(2)(3, 4)(5)()(6)))} ==
            R"("1" x x x "2" x x x "3, 4" x x x "5" x x x "" x x x "6")");
}
