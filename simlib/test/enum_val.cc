#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>

enum class Int32Enum : int32_t {
    A = 1,
    B = 42,
    C = 1337,
};

enum class Uint64Enum : uint64_t {
    A = 1,
    B = 42,
    C = 1337,
};

template <class Enum>
static void test_constructor_from_enum() {
    EnumVal<Enum> a = Enum::A;
    using UT = typename decltype(a)::ValType;
    static_assert(std::is_same_v<UT, std::underlying_type_t<Enum>>);
    EXPECT_EQ(a, Enum::A);
    EXPECT_NE(a, Enum::B);
}

// NOLINTNEXTLINE
TEST(EnumVal, constructor_from_enum) {
    test_constructor_from_enum<Int32Enum>();
    test_constructor_from_enum<Uint64Enum>();
}

template <class Enum>
static void test_constructor_from_val_type() {
    using UT = typename EnumVal<Enum>::ValType;
    EnumVal<Enum> a(static_cast<UT>(Enum::A));
    static_assert(
            not std::is_convertible_v<UT, decltype(a)>, "Implicit conversion is disallowed");
    static_assert(std::is_same_v<typename decltype(a)::ValType, UT>);
    EXPECT_EQ(a, Enum::A);
    EXPECT_NE(a, Enum::B);
}

// NOLINTNEXTLINE
TEST(EnumVal, constructor_value) {
    test_constructor_from_val_type<Int32Enum>();
    test_constructor_from_val_type<Uint64Enum>();
}

template <class Enum>
static void test_assignment_operator_from_enum() {
    EnumVal a = Enum::A;
    a = Enum::C;
    EXPECT_EQ(a, Enum::C);
    EXPECT_NE(a, Enum::A);
}

// NOLINTNEXTLINE
TEST(EnumVal, test_assignment_operator_from_enum) {
    test_assignment_operator_from_enum<Int32Enum>();
    test_assignment_operator_from_enum<Uint64Enum>();
}

template <class Enum>
static void test_assignment_operator_from_val_type() {
    EnumVal a = Enum::A;
    using UT = typename decltype(a)::ValType;
    a = EnumVal<Enum>(static_cast<UT>(Enum::C));
    EXPECT_EQ(a, Enum::C);
    EXPECT_NE(a, Enum::A);
}

// NOLINTNEXTLINE
TEST(EnumVal, assignment_operator_from_val_type) {
    test_assignment_operator_from_val_type<Int32Enum>();
    test_assignment_operator_from_val_type<Uint64Enum>();
}

template <class Enum>
static void test_int_val() {
    const EnumVal a = Enum::A;
    using UT = typename decltype(a)::ValType;
    static_assert(std::is_same_v<UT, decltype(a.to_int())>);
    EXPECT_EQ(a.to_int(), (int)Enum::A);
    EXPECT_NE(a.to_int(), (int)Enum::C);
}

// NOLINTNEXTLINE
TEST(EnumVal, to_int) {
    test_int_val<Int32Enum>();
    test_int_val<Uint64Enum>();
}

template <class Enum>
static void test_implicit_const_conversion() {
    const EnumVal a = Enum::A;
    using UT = typename decltype(a)::ValType;
    static_assert(
            not std::is_convertible_v<decltype((a)), UT>, "implicit conversion is disallowed");
    static_assert(std::is_constructible_v<const UT&, decltype((a))>);
    static_assert(not std::is_constructible_v<UT&&, decltype((a))>);
    static_assert(not std::is_constructible_v<UT&, decltype((a))>);
    EXPECT_EQ((const UT&)a, (UT)Enum::A);
    EXPECT_NE((const UT&)a, (UT)Enum::C);
}

// NOLINTNEXTLINE
TEST(EnumVal, operator_ValType_const) {
    test_implicit_const_conversion<Int32Enum>();
    test_implicit_const_conversion<Uint64Enum>();
}

template <class Enum>
static void test_implicit_conversion() {
    EnumVal a = Enum::A;
    using UT = typename decltype(a)::ValType;
    static_assert(
            not std::is_convertible_v<decltype((a)), UT>, "implicit conversion is disallowed");
    static_assert(std::is_constructible_v<const UT&, decltype((a))>);
    static_assert(not std::is_constructible_v<UT&&, decltype((a))>);
    static_assert(std::is_constructible_v<UT&, decltype((a))>);
    EXPECT_EQ((UT&)a, (UT)Enum::A);
    EXPECT_NE((UT&)a, (UT)Enum::C);
}

// NOLINTNEXTLINE
TEST(EnumVal, operator_ValType) {
    test_implicit_conversion<Int32Enum>();
    test_implicit_conversion<Uint64Enum>();
}

ENUM_WITH_STRING_CONVERSIONS(Colors, uint8_t,
    (RED, 1, "red")
    (GREEN, 4, "green")
    (BLUE, 88, "blue")
);

// NOLINTNEXTLINE
TEST(EnumVal, from_str) {
    EXPECT_EQ(EnumVal<Colors>::from_str("red"), Colors::RED);
    EXPECT_EQ(EnumVal<Colors>::from_str("red")->to_int(), 1);
    EXPECT_EQ(EnumVal<Colors>::from_str("green"), Colors::GREEN);
    EXPECT_EQ(EnumVal<Colors>::from_str("green")->to_int(), 4);
    EXPECT_EQ(EnumVal<Colors>::from_str("blue"), Colors::BLUE);
    EXPECT_EQ(EnumVal<Colors>::from_str("blue")->to_int(), 88);

    EXPECT_EQ(EnumVal<Colors>::from_str(""), std::nullopt);
    EXPECT_EQ(EnumVal<Colors>::from_str("bluee"), std::nullopt);
    EXPECT_EQ(EnumVal<Colors>::from_str("der"), std::nullopt);
}
