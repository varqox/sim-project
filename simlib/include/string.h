#pragma once

#include "likely.h"
#include "meta.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <memory>
#include <string>

#ifdef _GLIBCXX_DEBUG
# include <cassert>
#endif

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCAT(x, y) CONCATENATE_DETAIL(x, y)

#define throw_assert(expr) \
	((expr) ? (void)0 : throw std::runtime_error(std::string(__FILE__ ":" \
	STRINGIZE(__LINE__) ": ").append(__PRETTY_FUNCTION__).append( \
	": Assertion `" #expr "` failed.")))

template<class T>
constexpr inline auto string_length(const T& x) noexcept -> decltype(x.size()) {
	return x.size();
}

template<uint LEN, uint... Digits>
constexpr inline uint string_length(meta::ToStringHelper<LEN, Digits...>) {
	return LEN;
}

template<class T>
constexpr inline size_t string_length(T* x) noexcept {
	return __builtin_strlen(x);
}

constexpr inline size_t string_length(char) noexcept { return 1; }

template<class T>
constexpr inline auto data(const T& x) noexcept { return x.data(); }

// Some stuff to provide a mechanism to call std::copy on meta::ToString<>
template<uintmax_t N>
struct meta_to_string_helper_iter {
	meta_to_string_helper_iter() = default;
};

template<uintmax_t N, class T>
auto operator+(meta_to_string_helper_iter<N> x, T) { return x; }

template<uint LEN, uint... Digits, class OIter>
constexpr inline OIter copy_meta_to_string_helper(
	meta::ToStringHelper<LEN, Digits...>, OIter result)
{
	for (auto c : {Digits...})
		*result++ = '0' + c;

	return result;
}

namespace std {

template<uintmax_t N, class OIter>
constexpr inline OIter copy(meta_to_string_helper_iter<N>,
	meta_to_string_helper_iter<N>, OIter result)
{
	return copy_meta_to_string_helper(meta::ToString<N>{}, result);
}

} // namespace std

template<uintmax_t N>
constexpr inline auto data(meta::ToString<N>) {
	return meta_to_string_helper_iter<N>{};
}

template<class T, size_t N>
constexpr inline auto data(const T (&x)[N]) noexcept { return x; }

constexpr inline auto data(const char* const x) noexcept { return x; }

constexpr inline auto data(char* const x) noexcept { return x; }

constexpr inline const char* data(const char& c) noexcept { return &c; }

inline std::string& operator+=(std::string& str, const meta::string& s) {
	return str.append(s.data(), s.size());
}

template<uint LEN, uint... Digits>
inline std::string& operator+=(std::string& str,
	meta::ToStringHelper<LEN, Digits...>)
{
	(void)std::initializer_list<int>{(str += '0' + Digits, 0)...};
	return str;
}

template<size_t N>
class InplaceBuff;

// Only to use with standard integral types
// +1 for a terminating null char and +1 for the minus sign
template<class T, size_t N =
	string_length(meta::ToString<std::numeric_limits<T>::max()>{}) + 2>
constexpr inline auto toString(T x) noexcept ->
	std::enable_if_t<std::is_integral<T>::value, InplaceBuff<N>>;

template<class T>
inline auto stringify(T&& x) noexcept -> decltype(std::forward<T>(x))
{
	return std::forward<T>(x);
}

// Allows stringifying integers
inline auto stringify(bool x) noexcept {
	return (x ? "true" : "false");
}

inline char stringify(char x) noexcept { return x; }
inline auto stringify(unsigned char x) noexcept -> decltype(toString(x));

inline auto stringify(short x) noexcept -> decltype(toString(x));
inline auto stringify(unsigned short x) noexcept -> decltype(toString(x));

inline auto stringify(int x) noexcept -> decltype(toString(x));
inline auto stringify(unsigned x) noexcept -> decltype(toString(x));
inline auto stringify(long x) noexcept -> decltype(toString(x));
inline auto stringify(unsigned long x) noexcept -> decltype(toString(x));

inline auto stringify(long long x) noexcept -> decltype(toString(x));
inline auto stringify(unsigned long long x) noexcept -> decltype(toString(x));

/**
 * @brief Concentrates @p args into std::string
 *
 * @param args string-like objects
 *
 * @return concentration of @p args
 */
template<class... Args>
inline std::string concat_tostr(Args&&... args) {
	return [](auto&&... xx) {
		size_t total_length = 0;
		(void)std::initializer_list<int>{
			(total_length += string_length(xx), 0)...
		};

		std::string res;
		res.reserve(total_length);

		(void)std::initializer_list<int>{
			(res += std::forward<decltype(xx)>(xx), 0)...
		};

		return res;
	}(stringify(std::forward<Args>(args))...);
}

template<class Char>
class StringBase {
public:
	// Types
	using value_type = Char;
	using const_reference = const Char&;
	using reference = Char&;
	using pointer = Char*;
	using const_pointer = const Char*;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using size_type = size_t;

	static constexpr size_type npos = -1;

protected:
	pointer str;
	size_type len;

public:
	constexpr StringBase() noexcept : str(""), len(0) {}

	constexpr StringBase(std::nullptr_t) = delete;

	#if __cplusplus > 201402L
	#warning "Use below std::chair_traits<Char>::length() which became constexpr"
	#endif
	template<size_t N>
	constexpr StringBase(const char(&s)[N]) : str(s), len(meta::strlen(s)) {}

	// Do not treat as possible string literal
	template<size_t N>
	constexpr StringBase(char(&s)[N]) : str(s), len(__builtin_strlen(s)) {}

	constexpr StringBase(const meta::string& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase(pointer s) noexcept
		: str(s), len(__builtin_strlen(s)) {}

	// A prototype: template<N> ...(const char(&a)[N]){} is not used because it
	// takes const char[] wrongly (sets len to array size instead of stored
	// string's length)

	constexpr StringBase(pointer s, size_type n) noexcept : str(s), len(n) {}

	constexpr StringBase(std::string&&) noexcept = delete;

	constexpr StringBase(const std::string&&) noexcept = delete;

	// Constructs StringView from substring [beg, beg + n) of string s
	constexpr StringBase(const std::string& s, size_type beg = 0,
		size_type n = npos) noexcept
		: str(s.data() + std::min(beg, s.size())),
			len(std::min(n, s.size() - std::min(beg, s.size()))) {}

	constexpr StringBase(const StringBase& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase(StringBase<Char>&& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase& operator=(StringBase&& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	constexpr StringBase& operator=(const StringBase& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	constexpr StringBase& operator=(std::nullptr_t) = delete;

	~StringBase() = default;

	// Returns whether the StringBase is empty (size() == 0)
	constexpr bool empty() const noexcept { return (len == 0); }

	// Returns the number of characters in the StringBase
	constexpr size_type size() const noexcept { return len; }

	// Returns the number of characters in the StringBase
	constexpr size_type length() const noexcept { return len; }

	// Returns a pointer to the underlying character array
	constexpr pointer data() noexcept { return str; }

	// Returns a const pointer to the underlying character array
	constexpr const_pointer data() const noexcept { return str; }

	constexpr iterator begin() noexcept { return str; }

	constexpr iterator end() noexcept { return str + len; }

	constexpr const_iterator begin() const noexcept { return str; }

	constexpr const_iterator end() const noexcept { return str + len; }

	constexpr const_iterator cbegin() const noexcept { return str; }

	constexpr const_iterator cend() const noexcept { return str + len; }

	// Returns reference to first element
	constexpr reference front() noexcept { return str[0]; }

	// Returns const_reference to first element
	constexpr const_reference front() const noexcept { return str[0]; }

	// Returns reference to last element
	constexpr reference back() noexcept { return str[len - 1]; }

	// Returns const_reference to last element
	constexpr const_reference back() const noexcept { return str[len - 1]; }

	// Returns reference to n-th element
	constexpr reference operator[](size_type n) noexcept {
	#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
	#endif
		return str[n];
	}

	// Returns const_reference to n-th element
	constexpr const_reference operator[](size_type n) const noexcept {
	#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
	#endif
		return str[n];
	}

	// Like operator[] but throws exception if n >= size()
	constexpr reference at(size_type n) {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

	#ifdef _GLIBCXX_DEBUG
		assert(n >= 0);
		assert(n < len);
	#endif
		return str[n];
	}

	// Like operator[] but throws exception if n >= size()
	constexpr const_reference at(size_type n) const {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

		return str[n];
	}

	// Swaps two StringBase
	void swap(StringBase& s) noexcept {
		// Swap str
		pointer p = str;
		str = s.str;
		s.str = p;
		// Swap len
		size_type x = len;
		len = s.len;
		s.len = x;
	}

	/**
	 * @brief Compares two StringBase
	 *
	 * @param s StringBase to compare with
	 *
	 * @return <0 - this < @p s, 0 - equal, >0 - this > @p s
	 */
	constexpr int compare(const StringBase& s) const noexcept {
		size_type clen = std::min(len, s.len);
		int rc = memcmp(str, s.str, clen);
		return rc != 0 ? rc : (len == s.len ? 0 : ((len < s.len) ? -1 : 1));
	}

	constexpr int compare(size_type pos, size_type count, const StringBase& s)
		const
	{
		return substr(pos, count).compare(s);
	}

	// Returns position of the first character of the first substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type find(const StringBase& s) const noexcept {
		if (s.len == 0)
			return 0;

		// KMP algorithm
		std::unique_ptr<size_type[]> p {new size_type[s.len]};
		size_type k = p[0] = 0;
		// Fill p
		for (size_type i = 1; i < s.len; ++i) {
			while (k > 0 && s[i] != s[k])
				k = p[k - 1];
			if (s[i] == s[k])
				++k;
			p[i] = k;
		}

		k = 0;
		for (size_type i = 0; i < len; ++i) {
			while (k > 0 && str[i] != s[k])
				k = p[k - 1];
			if (str[i] == s[k]) {
				++k;
				if (k == s.len)
					return i - s.len + 1;
			}
		}

		return npos;
	}

	constexpr size_type find(const StringBase& s, size_type beg1) const {
		return find(s.substr(beg1));
	}

	constexpr size_type find(const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(size_type beg, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg).find(s.substr(beg1, len - beg1));
	}

	constexpr size_type find(size_type beg, const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return substr(beg).find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(size_type beg, size_type endi, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg, endi).find(s.substr(beg1, len - beg1));
	}

	constexpr size_type find(size_type beg, size_type endi, const StringBase& s,
		size_type beg1, size_type endi1) const
	{
		return substr(beg, endi).find(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type find(char c, size_type beg = 0) const noexcept {
		for (; beg < len; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	constexpr size_type find(char c, size_type beg, size_type endi) const
		noexcept
	{
		if (endi > len)
			endi = len;

		for (; beg < endi; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	// Returns position of the first character of the last substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type rfind(const StringBase& s) const noexcept {
		if (s.len == 0)
			return 0;

		// KMP algorithm
		std::unique_ptr<size_type[]> p {new size_type[s.len]};
		size_type slen1 = s.len - 1, k = p[slen1] = slen1;
		// Fill p
		for (size_type i = slen1 - 1; i != npos; --i) {
			while (k < slen1 && s[i] != s[k])
				k = p[k + 1];
			if (s[i] == s[k])
				--k;
			p[i] = k;
		}

		k = slen1;
		for (size_type i = len - 1; i != npos; --i) {
			while (k < slen1 && str[i] != s[k])
				k = p[k + 1];
			if (str[i] == s[k]) {
				--k;
				if (k == npos)
					return i;
			}
		}

		return npos;
	}

	constexpr size_type rfind(const StringBase& s, size_type beg1) const {
		return rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(size_type beg, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg).rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(size_type beg, const StringBase& s,
		size_type beg1, size_type endi1) const
	{
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(size_type beg, size_type endi,
		const StringBase& s, size_type beg1 = 0) const
	{
		return substr(beg, endi).rfind(s.substr(beg1, len - beg1));
	}

	constexpr size_type rfind(size_type beg, size_type endi,
		const StringBase& s, size_type beg1, size_type endi1) const
	{
		return substr(beg, endi).rfind(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(char c, size_type beg = 0) const noexcept {
		for (size_type endi = len; endi > beg; )
			if (str[--endi] == c)
				return endi;

		return npos;
	}

	constexpr size_type rfind(char c, size_type beg, size_type endi) const
		noexcept
	{
		if (endi > len)
			endi = len;

		for (; endi > beg; )
			if (str[--endi] == c)
				return endi;

		return npos;
	}

protected:
	// Returns a StringBase of the substring [pos, ...)
	constexpr StringBase substr(size_type pos) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, len - pos);
	}

	// Returns a StringBase of the substring [pos, pos + count)
	constexpr StringBase substr(size_type pos, size_type count) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, std::min(count, len - pos));
	}

	// Returns a StringBase of the substring [beg, ...)
	constexpr StringBase substring(size_type beg) const { return substr(beg); }

	constexpr StringBase substring(size_type beg, size_type endi) const {
		if (beg > endi || beg > len)
			throw std::out_of_range("StringBase::substring");

		return StringBase(str + beg, std::min(len, endi) - beg);
	}

public:
	std::string to_string() const { return std::string(str, len); }
};

template<class Char>
constexpr inline std::string& operator+=(std::string& str,
	const StringBase<Char>& s)
{
	return str.append(s.data(), s.size());
}

class StringView : public StringBase<const char> {
public:
	using StringBase::StringBase;

	constexpr StringView() noexcept : StringBase("", 0) {}

	constexpr StringView(std::nullptr_t) noexcept = delete;

	constexpr StringView(const StringView&) noexcept = default;
	constexpr StringView(StringView&&) noexcept = default;
	StringView& operator=(const StringView&) noexcept = default;
	StringView& operator=(StringView&&) noexcept = default;

	constexpr StringView& operator=(std::nullptr_t) noexcept = delete;

	constexpr StringView operator=(pointer p) noexcept {
		return operator=(StringView{p});
	}

	template<class T,
		typename = std::enable_if_t<std::is_rvalue_reference<T>::value>>
	StringView& operator=(T&&) = delete; // Protect from assigning unsafe data

	constexpr StringView(const StringBase& s) noexcept : StringBase(s) {}

	~StringView() = default;

	constexpr void clear() noexcept { len = 0; }

	// Removes prefix of length n
	constexpr StringView& removePrefix(size_type n) noexcept {
		if (n > len)
			n = len;
		str += n;
		len -= n;
		return *this;
	}

	// Removes suffix of length n
	constexpr StringView& removeSuffix(size_type n) noexcept {
		if (n > len)
			len = 0;
		else
			len -= n;
		return *this;
	}

	// Extracts prefix of length n
	constexpr StringView extractPrefix(size_type n) noexcept {
		if (n > len)
			n = len;

		StringView res = substring(0, n);
		str += n;
		len -= n;
		return res;
	}

	// Extracts suffix of length n
	constexpr StringView extractSuffix(size_type n) noexcept {
		if (n > len)
			len = n;
		len -= n;
		return {data() + len, n};
	}

	// Removes leading characters for which f() returns true
	template<class Func>
	constexpr StringView& removeLeading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}
		str += i;
		len -= i;
		return *this;
	}

	constexpr StringView& removeLeading(char c) noexcept {
		size_type i = 0;
		for (; i < len && str[i] == c; ++i) {}
		str += i;
		len -= i;
		return *this;
	}

	// Removes trailing characters for which f() returns true
	template<class Func>
	constexpr StringView& removeTrailing(Func&& f) {
		while (len > 0 && f(back()))
			--len;
		return *this;
	}

	constexpr StringView& removeTrailing(char c) noexcept {
		while (len > 0 and back() == c)
			--len;
		return *this;
	}

	// Extracts leading characters for which f() returns true
	template<class Func>
	constexpr StringView extractLeading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}

		StringView res = substring(0, i);
		str += i;
		len -= i;

		return res;
	}

	// Extracts trailing characters for which f() returns true
	template<class Func>
	constexpr StringView extractTrailing(Func&& f) {
		size_type i = len;
		for (; i > 0 && f(str[i - 1]); --i) {}

		StringView res = substring(i, len);
		len = i;

		return res;
	}

	constexpr StringView withoutPrefix(size_t n) noexcept {
		return StringView(*this).removePrefix(n);
	}

	constexpr StringView withoutSuffix(size_t n) noexcept {
		return StringView(*this).removeSuffix(n);
	}

#if __cplusplus > 201402L
#warning "Since C++17 constexpr can be used below"
#endif
	template<class T>
	/*constexpr*/ StringView withoutLeading(T&& arg) {
		return StringView(*this).removeLeading(std::forward<T>(arg));
	}

#if __cplusplus > 201402L
#warning "Since C++17 constexpr can be used below"
#endif
	template<class T>
	/*constexpr*/ StringView withoutTrailing(T&& arg) {
		return StringView(*this).removeTrailing(std::forward<T>(arg));
	}

	using StringBase::substr;
	using StringBase::substring;
};

inline std::string& operator+=(std::string& str, StringView s) {
	return str.append(s.data(), s.size());
}

inline StringView intentionalUnsafeStringView(const char* str) noexcept {
	return {str};
}

inline StringView intentionalUnsafeStringView(StringView str) noexcept {
	return str;
}

// This function allows std::string&& to be converted to StringView, but keep in
// mind that if any StringView or alike value generated from the result of this
// function cannot be saved to variable! - it would (and probably will) cause
// using of the provided std::string's data that was already deallocated =
// memory corruption
inline StringView intentionalUnsafeStringView(const std::string&& str) noexcept {
	return StringView(static_cast<const std::string&>(str));
}

// comparison operators
inline constexpr bool operator==(StringView a, StringView b) noexcept {
	return (a.size() == b.size() && memcmp(a.data(), b.data(), a.size()) == 0);
}

inline constexpr bool operator!=(StringView a, StringView b) noexcept {
	return (a.size() != b.size() || memcmp(a.data(), b.data(), a.size()) != 0);
}

inline constexpr bool operator<(StringView a, StringView b) noexcept {
	return (a.compare(b) < 0);
}

inline constexpr bool operator>(StringView a, StringView b) noexcept {
	return (a.compare(b) > 0);
}

inline constexpr bool operator<=(StringView a, StringView b) noexcept {
	return (a.compare(b) <= 0);
}

inline constexpr bool operator>=(StringView a, StringView b) noexcept {
	return (a.compare(b) >= 0);
}

// Holds string with fixed size and null-terminating character at the end
class FixedString : public StringBase<char> {
public:
	FixedString() : StringBase(new char[1](), 0) {}

	FixedString(std::nullptr_t) : FixedString() {}

	~FixedString() { delete[] str; }

	FixedString(size_type n, char c = '\0') : StringBase(new char[n + 1], n) {
		memset(str, c, n);
		str[len] = '\0';
	}

	// s cannot be nullptr
	FixedString(const_pointer s, size_type n) : StringBase(new char[n + 1], n) {
		memcpy(str, s, len);
		str[len] = '\0';
	}

	FixedString(const StringBase& s)
		: FixedString(s.data(), s.size()) {}

	FixedString(const StringBase<const value_type>& s)
		: FixedString(s.data(), s.size()) {}

	// s cannot be nullptr
	FixedString(const_pointer s) : FixedString(s, __builtin_strlen(s)) {}

	FixedString(const std::string& s, size_type beg = 0, size_type n = npos)
		: FixedString(s.data() + std::min(beg, s.size()),
			std::min(n, s.size() - std::min(beg, s.size()))) {}

	FixedString(const FixedString& fs) : FixedString(fs.str, fs.len) {}

	FixedString(FixedString&& fs) : StringBase(fs.str, fs.len) {
		fs.str = new char[1]();
		fs.len = 0;
	}

	FixedString& operator=(const FixedString& fs) {
		if (UNLIKELY(fs == *this))
			return *this;

		delete[] str;
		len = fs.len;
		str = new char[len + 1];
		memcpy(str, fs.str, len + 1);
		return *this;
	}

	FixedString& operator=(FixedString&& fs) {
		delete[] str;
		len = fs.len;
		str = fs.str;
		fs.len = 0;
		fs.str = new char[1]();
		return *this;
	}

	operator StringView() const { return {data(), size()}; }

	// Returns a FixedString of the substring [pos, ...)
	FixedString substr(size_type pos = 0) const {
		if (pos > len)
			throw std::out_of_range("FixedString::substr");

		return FixedString(str + pos, len - pos);
	}

	// Returns a FixedString of the substring [pos, pos + count)
	FixedString substr(size_type pos, size_type count) const {
		if (pos > len)
			throw std::out_of_range("FixedString::substr");

		return FixedString(str + pos, std::min(count, len - pos));
	}
};

class CStringView : public StringBase<const char> {
public:
	constexpr CStringView() : StringBase("", 0) {}

	constexpr CStringView(std::nullptr_t) : CStringView() {}

	#if __cplusplus > 201402L
	#warning "Use below std::chair_traits<Char>::length() which became constexpr"
	#endif
	template<size_t N>
	constexpr CStringView(const char(&s)[N]) : StringBase(s, meta::strlen(s)) {}

	// Do not treat as possible string literal
	template<size_t N>
	constexpr CStringView(char(&s)[N]) : StringBase(s) {}

	constexpr CStringView(const meta::string& s) : StringBase(s) {}

	constexpr CStringView(const FixedString& s) noexcept
		: StringBase(s.data(), s.size()) {}

	CStringView(const std::string& s) noexcept
		: StringBase(s.data(), s.size()) {}

	// Be careful with the constructor below! @p s cannot be null
	constexpr explicit CStringView(pointer s) noexcept : StringBase(s) {
	#ifdef _GLIBCXX_DEBUG
		assert(s);
	#endif
	}

	// Be careful with the constructor below! @p s cannot be null
	constexpr CStringView(pointer s, size_type n) noexcept : StringBase(s, n) {
	#ifdef _GLIBCXX_DEBUG
		assert(s);
		assert(s[n] == '\0');
	#endif
	}

	constexpr CStringView(const CStringView&) noexcept = default;
	constexpr CStringView(CStringView&&) noexcept = default;
	constexpr CStringView& operator=(const CStringView&) noexcept = default;
	constexpr CStringView& operator=(CStringView&&) noexcept = default;

	constexpr operator StringView() const { return {data(), size()}; }

	constexpr CStringView substr(size_type pos) const {
		const auto x = StringBase::substr(pos);
		return CStringView{x.data(), x.size()};
	}

	constexpr StringView substr(size_type pos, size_type count) const {
		return StringBase::substr(pos, count);
	}

	constexpr CStringView substring(size_type beg) const { return substr(beg); }

	constexpr StringView substring(size_type beg, size_type end) const {
		return StringBase::substring(beg, end);
	}

	constexpr const_pointer c_str() const noexcept { return data(); }
};

constexpr inline StringView substring(StringView str, StringView::size_type beg,
	StringView::size_type end = StringView::npos)
{
	return str.substring(beg, end);
}

template<class CharT, class Traits, class Char>
constexpr std::basic_ostream<CharT, Traits>& operator<<(
	std::basic_ostream<CharT, Traits>& os, const StringBase<Char>& s)
{
	return os.write(s.data(), s.size());
}

class InplaceBuffBase {
public:
	size_t size = 0;
	size_t max_size_;
	char *p_;

	/**
	 * @brief Changes buffer's size and max_size if needed, but in the latter
	 *   case all the data are lost
	 *
	 * @param n new buffer's size
	 */
	void lossy_resize(size_t n) {
		if (n > max_size_) {
			max_size_ = meta::max(max_size_ << 1, n);
			deallocate();
			try {
				p_ = new char[max_size_];
			} catch (...) {
				p_ = (char*)&p_ + sizeof(p_);
				max_size_ = 0;
				throw;
			}
		}

		size = n;
	}

	/**
	 * @brief Changes buffer's size and max_size if needed, preserves data
	 *
	 * @param n new buffer's size
	 */
	void resize(size_t n) {
		if (n > max_size_) {
			size_t new_ms = meta::max(max_size_ << 1, n);
			char* new_p = new char[new_ms];
			std::copy(p_, p_ + size, new_p);
			deallocate();

			p_ = new_p;
			max_size_ = new_ms;
		}

		size = n;
	}

	void clear() noexcept { size = 0; }

	char* begin() noexcept { return data(); }

	const char* begin() const noexcept { return data(); }

	char* end() noexcept { return data() + size; }

	const char* end() const noexcept { return data() + size; }

	const char* cbegin() const noexcept { return data(); }

	const char* cend() const noexcept { return data() + size; }

	char* data() noexcept { return p_; }

	const char* data() const noexcept { return p_; }

	size_t max_size() const noexcept { return max_size_; }

	char& front() noexcept { return (*this)[0]; }

	char front() const noexcept { return (*this)[0]; }

	char& back() noexcept { return (*this)[size - 1]; }

	char back() const noexcept { return (*this)[size - 1]; }

	char& operator[](size_t i) noexcept { return p_[i]; }

	char operator[](size_t i) const noexcept { return p_[i]; }

	operator StringView() const & noexcept { return {data(), size}; }

	// Do not allow to create StringView of a temporary object
	operator StringView() const && = delete;

	std::string to_string() const { return {data(), size}; }

	CStringView to_cstr() & {
		resize(size + 1);
		(*this)[--size] = '\0';
		return {data(), size};
	}

	// Do not allow to create CStringView of a temporary object
	CStringView to_cstr() && = delete;

protected:
	constexpr InplaceBuffBase(size_t s, size_t max_s, char* p) noexcept
		: size(s), max_size_(max_s), p_(p) {}

	constexpr InplaceBuffBase(const InplaceBuffBase&) noexcept = default;
	constexpr InplaceBuffBase(InplaceBuffBase&&) noexcept = default;
	InplaceBuffBase& operator=(const InplaceBuffBase&) noexcept = default;
	InplaceBuffBase& operator=(InplaceBuffBase&&) noexcept = default;

	bool is_allocated() const noexcept {
		// This is not pretty but works...
		return (p_ - (char*)&p_ != sizeof p_);
	}

	void deallocate() noexcept {
		if (is_allocated())
			delete[] p_;
	}

public:
	~InplaceBuffBase() { deallocate(); }

	template<class... Args>
	constexpr InplaceBuffBase& append(Args&&... args) {
		[this](auto&&... xx) {
			// Sum length of all args
			size_t k = size, final_len = size;
			(void)std::initializer_list<size_t>{
				(final_len += string_length(xx))...
			};
			resize(final_len);

			// Concentrate them into str[]
			auto impl_append = [&](auto&& s) {
				auto sl = string_length(s);
				std::copy(::data(s), ::data(s) + sl, data() + k);
				k += sl;
			};

			(void)impl_append; // Ignore warning 'unused' when no arguments are
			                   // provided
			(void)std::initializer_list<int>{
				(impl_append(std::forward<decltype(xx)>(xx)), 0)...
			};
		}(stringify(std::forward<Args>(args))...);

		return *this;
	}
};

template<size_t N>
class InplaceBuff : protected InplaceBuffBase {
private:
	std::array<char, N> a_;

	template<size_t M>
	friend class InplaceBuff;

	static_assert(N > 0, "Needed for accessing the array's 0-th element");

	using InplaceBuffBase::is_allocated;

public:
	using InplaceBuffBase::size;

	constexpr InplaceBuff() noexcept : InplaceBuffBase(0, N, nullptr) {
		p_ = &a_[0];
	}

	template<class T, typename = std::enable_if_t<std::is_same<T, size_t>::value>>
	constexpr explicit InplaceBuff(T n)
		: InplaceBuffBase(n, meta::max(N, n), nullptr)
	{
		p_ = (n <= N ? &a_[0] : new char[n]);
	}

	template<class T, typename =
		std::enable_if_t<!std::is_integral<std::remove_reference_t<T>>()>>
	constexpr explicit InplaceBuff(T&& str) : InplaceBuff(string_length(str)) {
		std::copy(::data(str), ::data(str) + size, p_);
	}

	constexpr InplaceBuff(const InplaceBuff& ibuff) : InplaceBuff(ibuff.size) {
		std::copy(ibuff.data(), ibuff.data() + ibuff.size, data());
	}

	constexpr InplaceBuff(InplaceBuff&& ibuff) noexcept
		: InplaceBuffBase(ibuff.size, meta::max(N, ibuff.max_size_), ibuff.p_)
	{
		if (ibuff.is_allocated()) {
			p_ = ibuff.p_;
			ibuff.size = 0;
			ibuff.max_size_ = N;
			ibuff.p_ = &ibuff.a_[0];

		} else {
			p_ = &a_[0];
			std::copy(ibuff.data(), ibuff.data() + ibuff.size, p_);
			ibuff.size = 0;
		}
	}

	template<size_t M>
	constexpr InplaceBuff(const InplaceBuff<M>& ibuff) : InplaceBuff(ibuff.size) {
		std::copy(ibuff.data(), ibuff.data() + ibuff.size, data());
	}

	template<size_t M>
	constexpr InplaceBuff(InplaceBuff<M>&& ibuff) noexcept
		: InplaceBuffBase(ibuff.size, N, ibuff.p_)
	{
		if (ibuff.size <= N) {
			p_ = &a_[0];
			// max_size_ = N;
			std::copy(ibuff.data(), ibuff.data() + ibuff.size, p_);
			// Deallocate ibuff memory
			if (ibuff.is_allocated())
				delete[] std::exchange(ibuff.p_, nullptr);

		} else if (ibuff.is_allocated()) {
			p_ = ibuff.p_; // Steal the allocated string
			max_size_ = ibuff.max_size_;
		} else {
			// N < ibuff.size <= M
			p_ = new char[ibuff.size];
			max_size_ = ibuff.size;
			std::copy(ibuff.data(), ibuff.data() + ibuff.size, p_);
		}

		// Take care of the ibuff's state
		ibuff.size = 0;
		ibuff.max_size_ = M;
		ibuff.p_ = &ibuff.a_[0];
	}

	template<class T>
	constexpr InplaceBuff& operator=(T&& str) {
		auto len = string_length(str);
		lossy_resize(len);
		std::copy(::data(str), ::data(str) + len, data());
		return *this;
	}

	constexpr InplaceBuff& operator=(const InplaceBuff& ibuff) {
		lossy_resize(ibuff.size);
		std::copy(ibuff.data(), ibuff.data() + ibuff.size, data());
		return *this;
	}

	template<size_t M>
	constexpr InplaceBuff& operator=(const InplaceBuff<M>& ibuff) {
		lossy_resize(ibuff.size);
		std::copy(ibuff.data(), ibuff.data() + ibuff.size, data());
		return *this;
	}

private:
	template<size_t M>
	constexpr InplaceBuff& assign_move_impl(InplaceBuff<M>&& ibuff) {
		if (ibuff.is_allocated() and ibuff.max_size() >= max_size()) {
			// Steal the allocated string
			if (is_allocated())
				delete[] p_;

			p_ = ibuff.p_;
			size = ibuff.size;
			max_size_ = ibuff.max_size_;

		} else {
			*this = ibuff; // Copy data
			if (ibuff.is_allocated())
				delete[] ibuff.p_;
		}

		// Take care of the ibuff's state
		ibuff.p_ = &ibuff.a_[0];
		ibuff.size = 0;
		ibuff.max_size_ = M;

		return *this;
	}

public:
	constexpr InplaceBuff& operator=(InplaceBuff&& ibuff) noexcept {
		return assign_move_impl(std::move(ibuff));
	}

	template<size_t M>
	constexpr InplaceBuff& operator=(InplaceBuff<M>&& ibuff) noexcept {
		return assign_move_impl(std::move(ibuff));
	}

	using InplaceBuffBase::lossy_resize;
	using InplaceBuffBase::resize;
	using InplaceBuffBase::clear;
	using InplaceBuffBase::begin;
	using InplaceBuffBase::end;
	using InplaceBuffBase::cbegin;
	using InplaceBuffBase::cend;
	using InplaceBuffBase::data;
	using InplaceBuffBase::max_size;
	using InplaceBuffBase::front;
	using InplaceBuffBase::back;
	using InplaceBuffBase::operator[];
	using InplaceBuffBase::operator StringView;
	using InplaceBuffBase::to_string;
	using InplaceBuffBase::to_cstr;
	using InplaceBuffBase::append;
};

template<size_t N>
inline std::string& operator+=(std::string& str, const InplaceBuff<N>& ibs) {
	return str.append(ibs.data(), ibs.size);
}

// This function allows InplaceBuff<N>&& to be converted to StringView, but keep
// in mind that if any StringView or alike value generated from the result of
// this function cannot be saved to variable! - it would (and probably will)
// cause using of the provided InplaceBuff<N>'s data that was already
// deallocated = memory corruption
template<size_t N>
inline StringView intentionalUnsafeStringView(const InplaceBuff<N>&& str) noexcept {
	return StringView(static_cast<const InplaceBuff<N>&>(str));
}

// This function allows InplaceBuff<N>&& to be converted to CStringView, but
// keep in mind that if any CStringView or alike value generated from the result
// of this function cannot be saved to variable! - it would (and probably will)
// cause using of the provided InplaceBuff<N>'s data that was already
// deallocated = memory corruption
template<size_t N>
inline CStringView intentionalUnsafeCStringView(InplaceBuff<N>&& str) noexcept {
	return static_cast<InplaceBuff<N>&>(str).to_cstr();
}

template<size_t T>
constexpr inline auto string_length(const InplaceBuff<T>& ibuff)
	-> decltype(ibuff.size)
{
	return ibuff.size;
}

// This type should NOT be returned from a function call
class FilePath {
	const char* str_;
	size_t size_;

public:
	constexpr FilePath(const FilePath&) noexcept = default;
	constexpr FilePath(FilePath&&) noexcept = default;

	constexpr FilePath(const char* str) noexcept
		: str_(str), size_(__builtin_strlen(str)) {}

	constexpr FilePath(const CStringView& str) noexcept
		: str_(str.c_str()), size_(str.size()) {}

	FilePath(const std::string& str) noexcept
		: str_(str.c_str()), size_(str.size()) {}

	template<size_t N>
	constexpr FilePath(InplaceBuff<N>& str) noexcept
		: str_(str.to_cstr().data()), size_(str.size) {}

	template<size_t N>
	constexpr FilePath(InplaceBuff<N>&& str) noexcept
		: str_(str.to_cstr().data()), size_(str.size) {}

	FilePath& operator=(const FilePath&) noexcept = delete;
	FilePath& operator=(FilePath&&) noexcept = delete;
	FilePath& operator=(const FilePath&&) noexcept = delete;

	FilePath& operator=(const char* str) noexcept {
		str_ = str;
		size_ = __builtin_strlen(str);
		return *this;
	}

	FilePath& operator=(const CStringView& str) noexcept {
		str_ = str.c_str();
		size_ = str.size();
		return *this;
	}

	FilePath& operator=(const std::string& str) noexcept {
		str_ = str.c_str();
		size_ = str.size();
		return *this;
	}

	template<size_t N>
	FilePath& operator=(InplaceBuff<N>& str) noexcept {
		str_ = str.to_cstr().data();
		size_ = str.size;
		return *this;
	}

	template<size_t N>
	FilePath& operator=(InplaceBuff<N>&& str) noexcept {
		str_ = str.to_cstr().data();
		size_ = str.size;
		return *this;
	}

	operator const char*() const noexcept { return str_; }

	CStringView to_cstr() const noexcept { return {str_, size_}; }

	std::string to_str() const noexcept { return std::string(str_, size_); }

	const char* data() const noexcept { return str_; }

	size_t size() const noexcept { return size_; }
};

/**
 * @brief Concentrates @p args into std::string
 *
 * @param args string-like objects
 *
 * @return concentration of @p args
 */
template<size_t IBUFF_SIZE = 4096, class... Args>
constexpr inline InplaceBuff<IBUFF_SIZE> concat(Args&&... args) {
	InplaceBuff<IBUFF_SIZE> res;
	res.append(std::forward<Args>(args)...);
	return res;
}

// Compares two StringView, but before comparing two characters modifies them
// with f()
template<class Func>
constexpr bool special_less(StringView a, StringView b, Func&& f) {
	size_t len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
		if (f(a[i]) != f(b[i]))
			return f(a[i]) < f(b[i]);

	return (b.size() > len);
}

// Checks whether two StringView are equal, but before comparing two characters
// modifies them with f()
template<class Func>
constexpr bool special_equal(StringView a, StringView b, Func&& f) {
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i)
		if (f(a[i]) != f(b[i]))
			return  false;

	return true;
}

// Checks whether lowered @p a is equal to lowered @p b
constexpr inline bool lower_equal(StringView a, StringView b) noexcept {
	return special_equal<int(int)>(a, b, tolower);
}

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	constexpr bool operator()(StringView a, StringView b) const {
		a.removeLeading('0');
		b.removeLeading('0');
		return (a.size() == b.size() ? a < b : a.size() < b.size());
	}
};

template<class Func>
class SpecialStrCompare {
	Func func;

public:
	SpecialStrCompare(Func f = {}) : func(std::move(f)) {}

	template<class A, class B>
	bool operator()(A&& a, B&& b) const {
		return special_less(std::forward<A>(a), std::forward<B>(b), func);
	}
};

struct LowerStrCompare : public SpecialStrCompare<int(*)(int)> {
	LowerStrCompare() : SpecialStrCompare(tolower) {}
};

template<class T, size_t N>
constexpr inline auto toString(T x) noexcept ->
	std::enable_if_t<std::is_integral<T>::value, InplaceBuff<N>>
{
	using RType = InplaceBuff<N>;
	static_assert(N >=
		string_length(meta::ToString<std::numeric_limits<T>::max()>{}) + 2,
		"Needed to be noexcept");

	if (x == 0) {
		RType res((size_t)1);
		res[0] = '0';
		return res;
	}

	RType buff;
	if (std::is_signed<T>::value && x < 0) {
		while (x) {
			T x2 = x / 10;
			buff[buff.size++] = x2 * 10 - x + '0';
			x = x2;
		}
		buff[buff.size++] = '-';

		std::reverse(buff.begin(), buff.end());
		return buff;
	}

	while (x) {
		T x2 = x / 10;
		buff[buff.size++] = x - x2 * 10 + '0';
		x = x2;
	}

	std::reverse(buff.begin(), buff.end());
	return buff;
}

// Allows stringifying integers
inline auto stringify(unsigned char x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(short x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(unsigned short x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(int x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(unsigned x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(long x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(unsigned long x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(long long x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

inline auto stringify(unsigned long long x) noexcept -> decltype(toString(x)) {
	return toString(x);
}

// Converts T to std::string
template<class T>
std::enable_if_t<!std::is_integral<T>::value, std::string> toString(T x) {
	if (x == T())
		return std::string(1, '0');

	std::string res;
	if (x < T()) {
		while (x) {
			T x2 = x / 10;
			res += static_cast<char>(x2 * 10 - x + '0');
			x = x2;
		}
		res += '-';

	} else {
		while (x) {
			T x2 = x / 10;
			res += static_cast<char>(x - x2 * 10 + '0');
			x = x2;
		}
	}

	std::reverse(res.begin(), res.end());
	return res;
}

// TODO
inline std::string toString(double x, int precision = 6) {
	std::array<char, 350> buff;
	int rc = snprintf(buff.data(), buff.size(), "%.*lf", precision, x);
	if (0 < rc && rc < static_cast<int>(buff.size()))
		return std::string(buff.data(), rc);

	return std::to_string(x);
}
inline std::string toString(long double x, int precision = 6) {
	std::array<char, 5050> buff;
	int rc = snprintf(buff.data(), buff.size(), "%.*Lf", precision, x);
	if (0 < rc && rc < static_cast<int>(buff.size()))
		return std::string(buff.data(), rc);

	return std::to_string(x);
}

// Alias to toString()
template<class... Args>
constexpr inline auto toStr(Args&&... args) {
	return toString(std::forward<Args>(args)...);
}

// Like strtou() but places the result number into x
inline int strToNum(std::string& x, StringView s) {
	for (char c : s)
		if (!isdigit(c))
			return -1;

	x = s.to_string();
	return s.size();
}

// Like strToNum() but ends on the first occurrence of @p c or @p s's end
inline int strToNum(std::string& x, StringView s, char c) {
	if (s.empty()) {
		x.clear();
		return 0;
	}

	size_t i = 0;
	for (; i < s.size() && s[i] != c; ++i)
		if (!isdigit(s[i]))
			return -1;

	x = s.substr(0, i).to_string();
	return i;
}

// Like string::find() but searches in [beg, end)
constexpr inline size_t find(StringView str, char c, size_t beg = 0) {
	return str.find(c, beg);
}

// Like string::find() but searches in [beg, end)
constexpr inline size_t find(StringView str, char c, size_t beg,
	size_t end)
{
	return str.find(c, beg, end);
}

// Compares two strings: @p str[beg, end) and @p s
constexpr inline int compare(StringView str, size_t beg, size_t end,
	StringView s) noexcept
{
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

// Compares @p str[pos, str.find(c, pos)) and @p s
constexpr inline int compareTo(StringView str, size_t pos, char c, StringView s)
	noexcept
{
	return compare(str, pos, str.find(c, pos), s);
}

// Returns true if str1[0...len-1] == str2[0...len-1], false otherwise
// Comparison always takes O(len) iterations (that is why it is slow);
// it applies to security
bool slowEqual(const char* str1, const char* str2, size_t len) noexcept;

inline bool slowEqual(StringView str1, StringView str2) noexcept {
	return slowEqual(str1.data(), str2.data(),
		std::min(str1.size(), str2.size())) && str1.size() == str2.size();
}

// Removes trailing characters for which f() returns true
template<class Func>
void removeTrailing(std::string& str, Func&& f) {
	auto it = str.end();
	while (it != str.begin())
		if (!f(*--it)) {
			++it;
			break;
		}
	str.erase(it, str.end());
}

/// Removes trailing @p c
inline void removeTrailing(std::string& str, char c) noexcept {
	removeTrailing(str, [c](char x) { return (x == c); });
}

inline std::string tolower(std::string str) {
	for (auto& c : str)
		c = tolower(c);
	return str;
}

constexpr inline int hextodec(int c) noexcept {
	return (c < 'A' ? c - '0' : 10 + c - (c >= 'a' ? 'a' : 'A'));
}

constexpr inline char dectoHex(int x) noexcept {
	return (x > 9 ? 'A' - 10 + x : x + '0');
}

constexpr inline char dectohex(int x) noexcept {
	return (x > 9 ? 'a' - 10 + x : x + '0');
}

/// Converts each byte of the first @p len bytes of @p str to two hex digits
/// using dectohex()
std::string toHex(const char* str, size_t len);

/// Converts each byte of @p str to two hex digits using dectohex()
inline std::string toHex(StringView str) {
	return toHex(str.data(), str.size());
}

// TODO: rewrite using InplaceBuff
std::string encodeURI(StringView str, size_t beg = 0,
	size_t end = StringView::npos);

/// Decodes URI, WARNING: it does not validate hex digits
template<size_t N = 4096>
InplaceBuff<N> decodeURI(StringView str) {
	InplaceBuff<N> res;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '%' and i + 2 < str.size()) {
			res.template append<char>(
				(hextodec(str[i + 1]) << 4) | hextodec(str[i + 2]));
			i += 2;
		} else if (str[i] == '+')
			res.append(' ');
		else
			res.append(str[i]);
	}

	return res;
}

constexpr inline bool hasPrefix(StringView str, StringView prefix) noexcept {
	return (str.compare(0, prefix.size(), prefix) == 0);
}

template<class Iter>
constexpr bool hasPrefixIn(StringView str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
constexpr inline bool hasPrefixIn(StringView str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

constexpr inline bool hasPrefixIn(StringView str,
	const std::initializer_list<StringView>& x) noexcept
{
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

constexpr inline bool hasSuffix(StringView str, StringView suffix)
	noexcept
{
	return (str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

template<class Iter>
constexpr bool hasSuffixIn(StringView str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasSuffix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
constexpr inline bool hasSuffixIn(StringView str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasSuffix(str, a))
			return true;

	return false;
}

constexpr inline bool hasSuffixIn(StringView str,
	const std::initializer_list<StringView>& x) noexcept
{
	for (auto&& a : x)
		if (hasSuffix(str, a))
			return true;

	return false;
}

// Escapes HTML unsafe character and appends it to @p str
inline void appendHtmlEscaped(std::string& str, char c) {
	switch (c) {
	// To preserve spaces use CSS: white-space: pre | pre-wrap;
	case '&':
		str += "&amp;";
		break;

	case '"':
		str += "&quot;";
		break;

	case '\'':
		str += "&apos;";
		break;

	case '<':
		str += "&lt;";
		break;

	case '>':
		str += "&gt;";
		break;

	default:
		str += c;
	}
}

// Escapes HTML unsafe character sequences and appends them to @p str
inline void appendHtmlEscaped(std::string& str, StringView s) {
	for (char c : s)
		appendHtmlEscaped(str, c);
}

// Escapes HTML unsafe character sequences
template<class T>
inline std::string htmlEscape(T&& s) {
	std::string res;
	appendHtmlEscaped(res, intentionalUnsafeStringView(std::forward<T>(s)));
	return res;
}

template<size_t N = 512, class... Args>
constexpr inline InplaceBuff<N> jsonStringify(Args&&... args) {
	InplaceBuff<N> res;
	res.append('"');
	auto safe_append = [&](auto&& arg) {
		auto p = ::data(arg);
		for (size_t i = 0, len = string_length(arg); i < len; ++i) {
			unsigned char c = p[i];
			if (c == '\"')
				res.append("\\\"");
			else if (c == '\n')
				res.append("\\n");
			else if (c == '\\')
				res.append("\\\\");
			else if (iscntrl(c))
				res.append("\\u00", dectohex(c >> 4), dectohex(c & 15));
			else
				res.template append<char>(c);
		}
	};

	(void)std::initializer_list<int>{
		(safe_append(stringify(std::forward<Args>(args))), 0)...
	};

	res.append('"');
	return res;
}

constexpr inline bool isAlnum(StringView s) noexcept {
	for (char c : s)
		if (not('A' <= c and c <= 'Z') and not('0' <= c and c <= '9') and
			not('a' <= c and c <= 'z'))
		{
			return false;
		}

	return true;
}

/**
 * @brief Check whether string @p s[beg, end) is an integer
 * @details Equivalent to check id string matches regex [+\-]?[0-9]+
 *   Notes:
 *   - empty string is not an integer
 *   - sign is not an integer
 *
 * @param s string
 * @param beg beginning
 * @param end position of first character not to check
 *
 * @return result - whether string @p s[beg, end) is an integer
 */

constexpr inline bool isInteger(StringView s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return false; // empty string is not a number

	if ((s[beg] == '-' || s[beg] == '+') && ++beg == end)
			return false; // sign is not a number

	for (; beg < end; ++beg)
		if (s[beg] < '0' || s[beg] > '9')
			return false;

	return true;
}

constexpr inline bool isDigit(StringView s) noexcept {
	if (s.empty())
		return false;

	for (char c : s)
		if (c < '0' || c > '9')
			return false;

	return true;
}

constexpr inline bool isReal(StringView s) noexcept {
	if (s.empty() || s.front() == '.')
		return false;

	size_t beg = 0;
	if ((s.front() == '-' || s.front() == '+') && ++beg == s.size())
			return false; // sign is not a number

	bool dot = false;
	for (; beg < s.size(); ++beg)
		if (s[beg] < '0' || s[beg] > '9') {
			if (s[beg] == '.' && !dot && beg + 1 < s.size())
				dot = true;
			else
				return false;
		}

	return true;
}

/// Checks whether string @p s consist only of digits and is not greater than
/// @p MAX_VAL
template<uintmax_t MAX_VAL>
constexpr inline bool isDigitNotGreaterThan(StringView s) noexcept {
	constexpr auto x = meta::ToString<MAX_VAL>::arr_value;
	return isDigit(s) && !StrNumCompare()({x.data(), x.size()}, s);
}

/// Checks whether string @p s consist only of digits and is not less than
/// @p MIN_VAL
template<uintmax_t MIN_VAL>
constexpr inline bool isDigitNotLessThan(StringView s) noexcept {
	constexpr auto x = meta::ToString<MIN_VAL>::arr_value;
	return isDigit(s) && !StrNumCompare()(s, {x.data(), x.size()});
}

/**
 * @brief Converts s: [beg, end) to @p T
 * @details if end > s.size() or end == StringView::npos then end = s.size()
 *   if beg > end then beg = end
 *   if x == nullptr then only validate
 *
 * @param s string to convert to int
 * @param x pointer to int where result (converted value) will be placed
 * @param beg position from which @p s is considered
 * @param end position to which (exclusively) @p s is considered
 * @return -1 if s: [beg, end) is not a number (empty string is not a number),
 *   otherwise return the number of characters parsed.
 *   Warning! If return value is -1 then x may not be assigned, so using x after
 *   unsuccessful call is not safe.
 */
template<class T>
constexpr int strtoi(StringView s, T& x, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return -1;

	bool minus = (s[beg] == '-');
	int res = 0;
	x = 0;
	if ((s[beg] == '-' || s[beg] == '+') && (++res, ++beg) == end)
			return -1; // sign is not a number
	for (size_t i = beg; i < end; ++i) {
		if (s[i] >= '0' && s[i] <= '9')
			x = x * 10 + s[i] - '0';
		else
			return -1;
	}

	if (minus)
		x = -x;

	return res + end - beg;
}

// Like strtoi() but assumes that @p s is a unsigned integer
template<class T>
constexpr int strtou(StringView s, T& x, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return -1;

	int res = 0;
	x = 0;
	if (s[beg] == '+' && (++res, ++beg) == end)
		return -1; // Sign is not a number

	for (size_t i = beg; i < end; ++i) {
		if (s[i] >= '0' && s[i] <= '9')
			x = x * 10 + s[i] - '0';
		else
			return -1;
	}

	return res + end - beg;
}

// Converts string to long long using strtoi()
constexpr inline long long strtoll(StringView s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtoi() won't parse all the
	// string, and may (but don't have to) return parsed value
	long long x = 0;
	strtoi(s, x, beg, end);
	return x;
}

// Converts string to unsigned long long using strtou()
constexpr inline unsigned long long strtoull(StringView s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtou() won't parse all the
	// string, and may (but don't have to) return parsed value
	unsigned long long x = 0;
	strtou(s, x, beg, end);
	return x;
}

// Converts digits @p str to @p T, WARNING: assumes that isDigit(str) == true
template<class T>
constexpr T digitsToU(StringView str) noexcept {
	T x = 0;
	for (char c : str)
		x = 10 * x + c - '0';
	return x;
}

enum Adjustment : uint8_t { LEFT, RIGHT };

/**
 * @brief Returns a padded string
 * @details Examples:
 *   paddedString("abc", 5) -> "  abc"
 *   paddedString("abc", 5, LEFT) -> "abc  "
 *   paddedString("1234", 7, RIGHT, '0') -> "0001234"
 *   paddedString("1234", 4, RIGHT, '0') -> "1234"
 *   paddedString("1234", 2, RIGHT, '0') -> "1234"
 *
 * @param s string
 * @param len length of result string
 * @param left whether adjust to left or right
 * @param filler character used to fill blank fields
 *
 * @return formatted string
 */
std::string paddedString(StringView s, size_t len, Adjustment adj = RIGHT,
	char filler = ' ');
