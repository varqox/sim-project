#include "simlib/enum_val.hh"

#include <gtest/gtest.h>

enum class IntEnum : int {
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

TEST(EnumVal, constructor_from_enum) {
	test_constructor_from_enum<IntEnum>();
	test_constructor_from_enum<Uint64Enum>();
}

template <class Enum>
static void test_constructor_from_val_type() {
	using UT = typename EnumVal<Enum>::ValType;
	EnumVal<Enum> a((UT)Enum::A);
	static_assert(not std::is_convertible_v<UT, decltype(a)>,
	              "Implicit conversion is disallowed");
	static_assert(std::is_same_v<typename decltype(a)::ValType, UT>);
	EXPECT_EQ(a, Enum::A);
	EXPECT_NE(a, Enum::B);
}

TEST(EnumVal, constructor_value) {
	test_constructor_from_val_type<IntEnum>();
	test_constructor_from_val_type<Uint64Enum>();
}

template <class Enum>
static void test_assignment_operator_from_enum() {
	EnumVal a = Enum::A;
	a = Enum::C;
	EXPECT_EQ(a, Enum::C);
	EXPECT_NE(a, Enum::A);
}

TEST(EnumVal, test_assignment_operator_from_enum) {
	test_assignment_operator_from_enum<IntEnum>();
	test_assignment_operator_from_enum<Uint64Enum>();
}

template <class Enum>
static void test_assignment_operator_from_val_type() {
	EnumVal a = Enum::A;
	using UT = typename decltype(a)::ValType;
	a = EnumVal<Enum>((UT)Enum::C);
	EXPECT_EQ(a, Enum::C);
	EXPECT_NE(a, Enum::A);
}

TEST(EnumVal, assignment_operator_from_val_type) {
	test_assignment_operator_from_val_type<IntEnum>();
	test_assignment_operator_from_val_type<Uint64Enum>();
}

template <class Enum>
static void test_int_val() {
	const EnumVal a = Enum::A;
	using UT = typename decltype(a)::ValType;
	static_assert(std::is_same_v<UT, decltype(a.int_val())>);
	EXPECT_EQ(a.int_val(), (int)Enum::A);
	EXPECT_NE(a.int_val(), (int)Enum::C);
}

TEST(EnumVal, int_val) {
	test_int_val<IntEnum>();
	test_int_val<Uint64Enum>();
}

template <class Enum>
static void test_implicit_const_conversion() {
	const EnumVal a = Enum::A;
	using UT = typename decltype(a)::ValType;
	static_assert(not std::is_convertible_v<decltype((a)), UT>,
	              "implicit conversion is disallowed");
	static_assert(std::is_constructible_v<const UT&, decltype((a))>);
	static_assert(not std::is_constructible_v<UT&&, decltype((a))>);
	static_assert(not std::is_constructible_v<UT&, decltype((a))>);
	EXPECT_EQ((const UT&)a, (UT)Enum::A);
	EXPECT_NE((const UT&)a, (UT)Enum::C);
}

TEST(EnumVal, operator_ValType_const) {
	test_implicit_const_conversion<IntEnum>();
	test_implicit_const_conversion<Uint64Enum>();
}

template <class Enum>
static void test_implicit_conversion() {
	EnumVal a = Enum::A;
	using UT = typename decltype(a)::ValType;
	static_assert(not std::is_convertible_v<decltype((a)), UT>,
	              "implicit conversion is disallowed");
	static_assert(std::is_constructible_v<const UT&, decltype((a))>);
	static_assert(not std::is_constructible_v<UT&&, decltype((a))>);
	static_assert(std::is_constructible_v<UT&, decltype((a))>);
	EXPECT_EQ((UT&)a, (UT)Enum::A);
	EXPECT_NE((UT&)a, (UT)Enum::C);
}

TEST(EnumVal, operator_ValType) {
	test_implicit_conversion<IntEnum>();
	test_implicit_conversion<Uint64Enum>();
}
