#pragma once

#include <array>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

/// Some stuff useful in the meta programming

using uint = unsigned;

namespace meta {

template <typename T>
class is_constexpr {
	typedef int true_type;
	typedef struct {
		true_type a, b;
	} false_type;

	template <typename U>
	static true_type helper(U&);

	template <typename U>
	static false_type helper(...);

public:
	enum { value = (sizeof(helper<T>(0)) == sizeof(true_type)) };
};

class string {
private:
	const char* const p;
	const size_t len;

public:
	constexpr string() : p(nullptr), len(0) {}

	constexpr string(const char* const str, size_t len1) : p(str), len(len1) {}

	template <size_t N>
	constexpr string(const char (&str)[N])
	   : p(str), len(std::char_traits<char>::length(str)) {}

	// Do not treat as possible string literal
	template <size_t N>
	constexpr string(char (&str)[N]) = delete;

	constexpr char operator[](size_t n) const {
		return n < len ? p[n] : throw std::out_of_range("");
	}

	constexpr const char* data() const { return p; }

	constexpr size_t size() const { return len; }

	constexpr size_t length() const { return len; }
};

template <class T, class U, uint N1, uint N2>
constexpr bool equal_helper(const T (&x)[N1], const U (&y)[N2], uint idx) {
	return (N1 == N2 &&
	        (idx == N1 || (x[idx] == y[idx] && equal_helper(x, y, idx + 1))));
}

template <class T, class U, uint N1, uint N2>
constexpr bool equal(const T (&x)[N1], const U (&y)[N2]) {
	return equal_helper(x, y, 0);
}

template <class T>
constexpr bool is_sorted_compare(T&& a, T&& b) {
	return (a < b);
}

constexpr bool is_sorted_compare(const string& a, const string& b,
                                 size_t idx = 0) {
	return (idx == a.size() ? (idx < b.size()) :
	                        // idx < a.size()
	           (idx == b.size()
	               ? false
	               : (a[idx] < b[idx] ||
	                  (a[idx] == b[idx] && is_sorted_compare(a, b, idx + 1)))));
}

template <class T, size_t N>
constexpr bool is_sorted_helper(const std::array<T, N>& arr, size_t idx) {
	return (idx + 1 >= N ? true :
	                     // arr[idx] <= arr[idx + 1]
	           (!is_sorted_compare(arr[idx + 1], arr[idx]) &&
	            is_sorted_helper(arr, idx + 1)));
}

template <class T, size_t N>
constexpr bool is_sorted(const std::array<T, N>& arr) {
	return is_sorted_helper(arr, 0);
}

template <uint LEN, uint... Digits>
struct ToStringHelper {
	static constexpr const char value[] = {('0' + Digits)..., '\0'};
	static constexpr std::array<char, LEN> arr_value = {{('0' + Digits)...}};
};

template <uintmax_t N, uint LEN, uint... Digits>
struct ToStringDecompose
   : ToStringDecompose<N / 10, LEN + 1, N % 10, Digits...> {};

template <uint LEN, uint... Digits>
struct ToStringDecompose<0, LEN, Digits...> : ToStringHelper<LEN, Digits...> {};
// Spacial case: N == 0
template <>
struct ToStringDecompose<0, 0> : ToStringHelper<1, 0> {};

template <uintmax_t N>
struct ToString : ToStringDecompose<N, 0> {};

template <uintmax_t N>
constexpr const char* toStr = ToString<N>::value;

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

template <typename T1, typename T2>
constexpr size_t offset_of(T1 T2::*member) {
	constexpr T2 object {};
	return size_t(&(object.*member)) - size_t(&object);
}

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
