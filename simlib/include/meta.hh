#pragma once

#include <array>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

/// Some stuff useful in the meta programming

using uint = unsigned;

namespace meta {

template <class T>
constexpr T max(T&& x) {
	return x;
}

template <class T, class U>
constexpr typename std::common_type<T, U>::type max(T&& x, U&& y) {
	using CT = typename std::common_type<T, U>::type;
	CT xx(std::forward<T>(x));
	CT yy(std::forward<U>(y));
	return (xx > yy ? xx : yy);
}

template <class T, class... Args>
constexpr typename std::common_type<T, Args...>::type max(T&& x,
                                                          Args&&... args) {
	return max(std::forward<T>(x), max(std::forward<Args>(args)...));
}

template <class T>
constexpr T min(T&& x) {
	return x;
}

template <class T, class U>
constexpr typename std::common_type<T, U>::type min(T&& x, U&& y) {
	using CT = typename std::common_type<T, U>::type;
	CT xx(std::forward<T>(x));
	CT yy(std::forward<U>(y));
	return (xx < yy ? xx : yy);
}

template <class T, class... Args>
constexpr typename std::common_type<T, Args...>::type min(T&& x,
                                                          Args&&... args) {
	return min(std::forward<T>(x), min(std::forward<Args>(args)...));
}

template <intmax_t x, intmax_t... ints>
constexpr intmax_t sum = x + sum<ints...>;

template <intmax_t x>
constexpr intmax_t sum<x> = x;

template <size_t beg, size_t count, size_t... indexes>
struct create_span_impl {
	using type =
	   typename create_span_impl<beg + 1, count - 1, indexes..., beg>::type;
};

template <size_t beg, size_t... indexes>
struct create_span_impl<beg, 0, indexes...> {
	using type = std::index_sequence<indexes...>;
};

// Equal to std::index_sequence<beg, beg + 1, ..., end - 1>
template <size_t beg, size_t end>
using span = typename create_span_impl<beg, end - beg>::type;

} // namespace meta

#define DECLARE_ENUM_OPERATOR(enu, oper)                                       \
	constexpr enu operator oper(enu a, enu b) {                                \
		using UT = std::underlying_type<enu>::type;                            \
		return static_cast<enu>(static_cast<UT>(a) oper static_cast<UT>(b));   \
	}

#define DECLARE_ENUM_ASSIGN_OPERATOR(enu, oper)                                \
	constexpr enu& operator oper(enu& a, enu b) {                              \
		using UT = std::underlying_type<enu>::type;                            \
		UT x = static_cast<UT>(a);                                             \
		return (a = static_cast<enu>(x oper static_cast<UT>(b)));              \
	}

#define DECLARE_ENUM_UNARY_OPERATOR(enu, oper)                                 \
	constexpr enu operator oper(enu a) {                                       \
		using UT = std::underlying_type<enu>::type;                            \
		return static_cast<enu>(oper static_cast<UT>(a));                      \
	}

#define DECLARE_ENUM_COMPARE1(enu, oper)                                       \
	constexpr bool operator oper(enu a, std::underlying_type<enu>::type b) {   \
		return static_cast<decltype(b)>(a) oper b;                             \
	}

#define DECLARE_ENUM_COMPARE2(enu, oper)                                       \
	constexpr bool operator oper(std::underlying_type<enu>::type a, enu b) {   \
		return a oper static_cast<decltype(a)>(b);                             \
	}
