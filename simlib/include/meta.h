#pragma once

#include <array>

/// Some stuff useful in meta programming

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

class string {
private:
	const char* const p;
	const size_t len;

public:
	constexpr string() : p(nullptr), len(0) {}

	constexpr string(const char* const str, size_t len1) : p(str), len(len1) {}

	template<size_t N>
	constexpr string(const char(&a)[N]) : p(a), len(N - 1) {}

	constexpr char operator[](size_t n) const {
		return n < len ? p[n] :
			throw std::out_of_range("");
	}

	constexpr const char* data() const { return p; }

	constexpr size_t size() const { return len; }
};

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


#if __cplusplus > 201103L
# warning "Delete the classes Seq and GenSeq below (there are these features in"
	"C++14 - see std::integer_sequence)"
#endif

template<size_t... Idx>
struct Seq {};

template<size_t N, size_t... Idx>
struct GenSeq : GenSeq<N - 1, N - 1, Idx...> {};

template<size_t... Idx>
struct GenSeq<0, Idx...> : Seq<Idx...> {};

} // namespace meta
