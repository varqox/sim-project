#pragma once

#include <array>
#include <cerrno>
#include <cstdlib>
#include <type_traits>

/// Some stuff useful in the meta programming

using uint = unsigned;

namespace meta {

template <typename T>
class is_constexpr {
	typedef int true_type;
	typedef struct { true_type a, b; } false_type;

	template <typename U>
	static true_type helper(U&);

	template <typename U>
	static false_type helper(...);

public:
	enum {value = (sizeof(helper<T>(0)) == sizeof(true_type))};
};

constexpr inline size_t strlen(const char* const p) {
	size_t x = 0;
	while (p[x] != '\0')
		++x;
	return x;
}

class string {
private:
	const char* const p;
	const size_t len;

public:
	constexpr string() : p(nullptr), len(0) {}

	constexpr string(const char* const str, size_t len1) : p(str), len(len1) {}

	#if __cplusplus > 201402L
	#warning "Use below std::chair_traits<Char>::length() which became constexpr"
	#endif
	template<size_t N>
	constexpr string(const char (&str)[N]) : p(str), len(strlen(str)) {}

	// Do not treat as possible string literal
	template<size_t N>
	constexpr string(char (&str)[N]) = delete;

	constexpr char operator[](size_t n) const {
		return n < len ? p[n] :
			throw std::out_of_range("");
	}

	constexpr const char* data() const { return p; }

	constexpr size_t size() const { return len; }

	constexpr size_t length() const { return len; }
};

template<class T, class U, uint N1, uint N2>
constexpr bool equal_helper(const T (&x)[N1], const U (&y)[N2], uint idx) {
	return (N1 == N2 &&
		(idx == N1 || (x[idx] == y[idx] && equal_helper(x, y, idx + 1))));
}

template<class T, class U, uint N1, uint N2>
constexpr bool equal(const T (&x)[N1], const U (&y)[N2]) {
	return equal_helper(x, y, 0);
}

template<class T>
constexpr bool is_sorted_compare(T&& a, T&& b) { return (a < b); }

constexpr bool is_sorted_compare(
	const string& a, const string& b, size_t idx = 0)
{
	return (idx == a.size() ? (idx < b.size()) :
		// idx < a.size()
		(idx == b.size() ? false :
			(a[idx] < b[idx] ||
				(a[idx] == b[idx] && is_sorted_compare(a, b, idx + 1)))));
}

template<class T, size_t N>
constexpr bool is_sorted_helper(const std::array<T, N>& arr, size_t idx) {
	return (idx + 1 >= N ? true :
		// arr[idx] <= arr[idx + 1]
		(!is_sorted_compare(arr[idx + 1], arr[idx]) &&
					is_sorted_helper(arr, idx + 1)));
}

template<class T, size_t N>
constexpr bool is_sorted(const std::array<T, N>& arr) {
	return is_sorted_helper(arr, 0);
}

template<uint LEN, uint... Digits>
struct ToStringHelper {
	constexpr static const char value[] = {('0' + Digits)..., '\0'};
	constexpr static std::array<char, LEN> arr_value = {{('0' + Digits)...}};
};

template<uintmax_t N, uint LEN, uint... Digits>
struct ToStringDecompose : ToStringDecompose<N / 10, LEN + 1, N % 10, Digits...> {};

template<uint LEN, uint... Digits>
struct ToStringDecompose<0, LEN, Digits...> : ToStringHelper<LEN, Digits...> {};
// Spacial case: N == 0
template<>
struct ToStringDecompose<0, 0> : ToStringHelper<1, 0> {};

template<uintmax_t N>
struct ToString : ToStringDecompose<N, 0> {};

template<class T>
constexpr T max(T&& x) { return x; }

template<class T, class U>
constexpr typename std::common_type<T, U>::type max(T&& x, U&& y) {
	using CT = typename std::common_type<T, U>::type;
	return (static_cast<CT>(x) > static_cast<CT>(y) ? static_cast<CT>(x)
		: static_cast<CT>(y));
}

template<class T, class... Args>
constexpr typename std::common_type<T, Args...>::type max(T&& x, Args&&... args)
{
	return max(x, max(std::forward<Args>(args)...));
}

template<class T>
constexpr T min(T&& x) { return x; }

template<class T, class U>
constexpr typename std::common_type<T, U>::type min(T&& x, U&& y) {
	return (x < y ? x : y);
}

template<class T, class... Args>
constexpr typename std::common_type<T, Args...>::type min(T&& x, Args&&... args)
{
	return min(x, min(std::forward<Args>(args)...));
}

template<intmax_t x, intmax_t... ints>
constexpr intmax_t sum = x + sum<ints...>;

template<intmax_t x>
constexpr intmax_t sum<x> = x;

} // namespace meta
