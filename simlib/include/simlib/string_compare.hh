#pragma once

#include "simlib/string_traits.hh"
#include "simlib/string_view.hh"

#include <utility>

// Compares two StringView, but before comparing two characters modifies them
// with f()
template <class Func>
constexpr bool special_less(StringView a, StringView b, Func&& f) {
	size_t len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i) {
		if (f(a[i]) != f(b[i])) {
			return f(a[i]) < f(b[i]);
		}
	}

	return (b.size() > len);
}

// Checks whether two StringView are equal, but before comparing two characters
// modifies them with f()
template <class Func>
constexpr bool special_equal(StringView a, StringView b, Func&& f) {
	if (a.size() != b.size()) {
		return false;
	}

	for (size_t i = 0; i < a.size(); ++i) {
		if (f(a[i]) != f(b[i])) {
			return false;
		}
	}

	return true;
}

// Checks whether lowered @p a is equal to lowered @p b
constexpr bool lower_equal(StringView a, StringView b) noexcept {
	return special_equal<int(int)>(a, b, to_lower);
}

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	constexpr bool operator()(StringView a, StringView b) const {
		a.remove_leading('0');
		b.remove_leading('0');
		return (a.size() == b.size() ? a < b : a.size() < b.size());
	}
};

/**
 * @brief Works almost like strverscmp(3)
 */
struct StrVersionCompare {
	constexpr bool operator()(StringView a, StringView b) const {
		size_t pf = 0;
		while (pf < a.size() and pf < b.size() and a[pf] == b[pf]) {
			++pf;
		}

		bool adigit = pf < a.size() and is_digit(a[pf]);
		bool bdigit = pf < b.size() and is_digit(b[pf]);
		if (not adigit and not bdigit) {
			return a.substring(pf) < b.substring(pf);
		}

		size_t beg = pf;
		while (beg > 0 and is_digit(a[beg - 1])) {
			--beg;
		}
		StringView numa = a.substring(beg).extract_leading(is_digit<char>);
		StringView numb = b.substring(beg).extract_leading(is_digit<char>);

		// Digit is lower than any non-digit even empty string
		if (numa.empty() or numb.empty()) {
			return a.substring(pf) < b.substring(pf);
		}

		auto remove_leading_zeros = [](StringView& str) {
			size_t res = str.size();
			while (not str.empty() and str.front() == '0' and str != "0") {
				str.remove_prefix(1);
			}
			return res - str.size();
		};
		size_t a_leading_zeros = remove_leading_zeros(numa);
		size_t b_leading_zeros = remove_leading_zeros(numb);

		if (a_leading_zeros == 0 and b_leading_zeros == 0) {
			// Normal numbers -- compare as such
			return numa.size() == numb.size() ? numa < numb
											  : numa.size() < numb.size();
		}
		if (a_leading_zeros == 0) {
			assert(b_leading_zeros > 0);
			return false;
		}
		if (b_leading_zeros == 0) {
			assert(a_leading_zeros > 0);
			return true;
		}

		return a_leading_zeros == b_leading_zeros
			? numa < numb
			: a_leading_zeros > b_leading_zeros;
	}
};

template <class Func>
class SpecialStrCompare {
	Func func;

public:
	explicit SpecialStrCompare(Func f)
	: func(std::move(f)) {}

	template <class A, class B>
	bool operator()(A&& a, B&& b) const {
		return special_less(std::forward<A>(a), std::forward<B>(b), func);
	}
};

struct LowerStrCompare : public SpecialStrCompare<int (*)(int)> {
	LowerStrCompare()
	: SpecialStrCompare(to_lower) {}
};

// Compares two strings: @p str[beg, end) and @p s
constexpr int compare(
	const StringView& str, size_t beg, size_t end,
	const StringView& s) noexcept {
	if (end > str.size()) {
		end = str.size();
	}
	if (beg > end) {
		beg = end;
	}

	return str.compare(beg, end - beg, s);
}

// Compares @p str[pos, str.find(c, pos)) and @p s
constexpr int compare_to(
	const StringView& str, size_t pos, char c, const StringView& s) noexcept {
	return compare(str, pos, str.find(c, pos), s);
}

// Returns true if str1[0...len-1] == str2[0...len-1], false otherwise
// Comparison always takes O(len) iterations (that is why it is slow);
// it applies to security
bool slow_equal(const char* str1, const char* str2, size_t len) noexcept;

inline bool slow_equal(StringView str1, StringView str2) noexcept {
	return slow_equal(
			   str1.data(), str2.data(), std::min(str1.size(), str2.size())) &&
		str1.size() == str2.size();
}

/// Checks whether string @p s consist only of digits and is not greater than
/// @p MAX_VAL
template <uintmax_t MAX_VAL>
constexpr bool is_digit_not_greater_than(const StringView& s) noexcept {
	constexpr auto x = to_string(MAX_VAL);
	return (is_digit(s) and not StrNumCompare()(x, s));
}

/// Checks whether string @p s consist only of digits and is not less than
/// @p MIN_VAL
template <uintmax_t MIN_VAL>
constexpr bool is_digit_not_less_than(const StringView& s) noexcept {
	constexpr auto x = to_string(MIN_VAL);
	return (is_digit(s) and not StrNumCompare()(s, x));
}
