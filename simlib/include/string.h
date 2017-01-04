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
#  include <cassert>
#endif

template<size_t N>
class StringBuff {
public:
	static constexpr size_t max_size = N - 1;
	static_assert(N > 0, "max_size would underflow");

	char str[N];
	unsigned len = 0; // Not size_t; who would make so incredibly large buffer?

	constexpr StringBuff() = default;

	constexpr StringBuff(unsigned count, char ch);

	// Variadic constructor, it does not accept an integer as the first argument
	template<class Arg1, class... Args, typename =
		typename std::enable_if<!std::is_integral<Arg1>::value, void>::type>
	constexpr StringBuff(Arg1&& arg1, Args&&... args) {
		append(std::forward<Arg1>(arg1), std::forward<Args>(args)...);
	}

	constexpr bool empty() const noexcept { return (len == 0); }

	constexpr unsigned size() const noexcept { return len; }

	/// Appends without adding the null terminating character
	template<class... Args>
	StringBuff& raw_append(Args&&... args);

	/// Appends and adds the null terminating character
	template<class... Args>
	StringBuff& append(Args&&... args) {
		raw_append(std::forward<Args>(args)...);
		str[len] = '\0'; // Terminating null character
		return *this;
	}

	char* data() noexcept { return str; }

	constexpr const char* data() const noexcept { return str; }

	char& operator[](unsigned n) noexcept { return str[n]; }

	constexpr char& operator[](unsigned n) const noexcept { return str[n]; }
};

template<size_t N>
inline std::string& operator+=(std::string& s, const StringBuff<N>& sb) {
	return s.append(sb.str, sb.len);
}

template<size_t... N>
constexpr auto merge(const StringBuff<N>&... args) {
	return StringBuff<meta::sum<N...>>{args...};
}

template<size_t N>
constexpr size_t StringBuff<N>::max_size;

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

	template<size_t N>
	constexpr StringBase(const StringBuff<N>& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase(pointer s) noexcept
		: str(s), len(__builtin_strlen(s)) {}

	// A prototype: template<N> ...(const char(&a)[N]){} is not used because it
	// takes const char[] wrongly (sets len to array size instead of stored
	// string's length)

	constexpr StringBase(pointer s, size_type n) noexcept : str(s), len(n) {}

	// Constructs StringView from substring [beg, beg + n) of string s
	constexpr StringBase(const std::string& s, size_type beg = 0,
		size_type n = npos) noexcept
		: str(s.data() + std::min(beg, s.size())),
			len(std::min(n, s.size() - std::min(beg, s.size()))) {}

	constexpr StringBase(const StringBase<Char>& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase(StringBase<Char>&& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase& operator=(StringBase<Char>&& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	constexpr StringBase& operator=(const StringBase<Char>& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	~StringBase() = default;

	// Returns whether the StringBase is empty (size() == 0)
	constexpr bool empty() const noexcept { return (len == 0); }

	// Returns the number of characters in the StringBase
	constexpr size_type size() const noexcept { return len; }

	// Returns the number of characters in the StringBase
	constexpr size_type length() const noexcept { return len; }

	// Returns a pointer to the underlying character array
	pointer data() noexcept { return str; }

	// Returns a const pointer to the underlying character array
	constexpr const_pointer data() const noexcept { return str; }

	iterator begin() noexcept { return str; }

	iterator end() noexcept { return str + len; }

	constexpr const_iterator begin() const noexcept { return str; }

	constexpr const_iterator end() const noexcept { return str + len; }

	constexpr const_iterator cbegin() const noexcept { return str; }

	constexpr const_iterator cend() const noexcept { return str + len; }

	// Returns reference to first element
	reference front() noexcept { return str[0]; }

	// Returns const_reference to first element
	constexpr const_reference front() const noexcept { return str[0]; }

	// Returns reference to last element
	reference back() noexcept { return str[len - 1]; }

	// Returns const_reference to last element
	constexpr const_reference back() const noexcept { return str[len - 1]; }

	// Returns reference to n-th element
	reference operator[](size_type n) noexcept {
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
	reference at(size_type n) {
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
	virtual void swap(StringBase& s) noexcept {
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

	size_type find(const StringBase& s, size_type beg1) const {
		return find(s.substr(beg1, len - beg1));
	}

	size_type find(const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg).find(s.substr(beg1, len - beg1));
	}

	size_type find(size_type beg, const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return substr(beg).find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, size_type endi, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg, endi).find(s.substr(beg1, len - beg1));
	}

	size_type find(size_type beg, size_type endi, const StringBase& s,
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

	size_type rfind(const StringBase& s, size_type beg1) const {
		return rfind(s.substr(beg1, len - beg1));
	}

	size_type rfind(const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg).rfind(s.substr(beg1, len - beg1));
	}

	size_type rfind(size_type beg, const StringBase& s,
		size_type beg1, size_type endi1) const
	{
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, size_type endi,
		const StringBase& s, size_type beg1 = 0) const
	{
		return substr(beg, endi).rfind(s.substr(beg1, len - beg1));
	}

	size_type rfind(size_type beg, size_type endi,
		const StringBase& s, size_type beg1, size_type endi1) const
	{
		return substr(beg, endi).rfind(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	constexpr size_type rfind(char c, size_type beg = 0) const noexcept {
		for (size_type endi = len; endi > beg;)
			if (str[--endi] == c)
				return endi;

		return npos;
	}

	constexpr size_type rfind(char c, size_type beg, size_type endi) const
		noexcept
	{
		if (endi > len)
			endi = len;

		for (; endi > beg;)
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

	// comparison operators
	friend constexpr bool operator==(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.len == b.len && memcmp(a.str, b.str, a.len) == 0);
	}

	friend constexpr bool operator!=(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.len != b.len || memcmp(a.str, b.str, a.len) != 0);
	}

	friend constexpr bool operator<(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.compare(b) < 0);
	}

	friend constexpr bool operator>(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.compare(b) > 0);
	}

	friend constexpr bool operator<=(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.compare(b) <= 0);
	}

	friend constexpr bool operator>=(const StringBase& a, const StringBase& b)
		noexcept
	{
		return (a.compare(b) >= 0);
	}
};

// Holds string with fixed size and null-terminating character at the end
class FixedString : public StringBase<char> {
public:
	FixedString() : StringBase(new char[1](), 0) {}

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

class StringView : public StringBase<const char> {
public:
	using StringBase::StringBase;

	constexpr StringView(const FixedString& s) noexcept
		: StringBase(s.data(), s.size()) {}

	constexpr StringView() noexcept : StringBase("", 0) {}

	constexpr StringView(const StringView& s) noexcept = default;
	constexpr StringView(StringView&& s) noexcept = default;
	StringView& operator=(const StringView& s) noexcept = default;
	StringView& operator=(StringView&& s) noexcept = default;

	constexpr StringView(const StringBase& s) noexcept : StringBase(s) {}

	~StringView() = default;

	constexpr void clear() noexcept { len = 0; }

	// Removes prefix of length n
	constexpr void removePrefix(size_type n) noexcept {
		if (n > len)
			n = len;
		str += n;
		len -= n;
	}

	// Removes suffix of length n
	constexpr void removeSuffix(size_type n) noexcept {
		if (n > len)
			len = 0;
		else
			len -= n;
	}

	// Extracts prefix of length n
	StringView extractPrefix(size_type n) noexcept {
		if (n > len)
			n = len;

		StringView res = substring(0, n);
		str += n;
		len -= n;
		return res;
	}

	// Extracts suffix of length n
	StringView extractSuffix(size_type n) noexcept {
		if (n > len)
			len = n;
		len -= n;
		return {data() + len, n};
	}

	// Removes leading characters for which f() returns true
	template<class Func>
	constexpr void removeLeading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}
		str += i;
		len -= i;
	}

	constexpr void removeLeading(char c) noexcept {
		removeLeading([c](char x) { return (x == c); });
	}

	// Removes trailing characters for which f() returns true
	template<class Func>
	constexpr void removeTrailing(Func&& f) {
		while (len > 0 && f(back()))
			--len;
	}

	constexpr void removeTrailing(char c) noexcept {
		removeTrailing([c](char x) { return (x == c); });
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

	using StringBase::substr;
	using StringBase::substring;
};

class CStringView : public StringBase<const char> {
public:
	constexpr CStringView() : StringBase("", 0) {}

	#if __cplusplus > 201402L
	#warning "Use below std::chair_traits<Char>::length() which became constexpr"
	#endif
	template<size_t N>
	constexpr CStringView(const char(&s)[N]) : StringBase(s, meta::strlen(s)) {}

	// Do not treat as possible string literal
	template<size_t N>
	constexpr CStringView(char(&s)[N]) : StringBase(s) {}

	constexpr CStringView(const meta::string& s) : StringBase(s) {}

	template<size_t N>
	constexpr CStringView(const StringBuff<N>& s) : StringBase(s) {}

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
	constexpr explicit CStringView(pointer s, size_type n) noexcept
		: StringBase(s, n)
	{
	#ifdef _GLIBCXX_DEBUG
		assert(s);
		assert(s[n] == '\0');
	#endif
	}

	constexpr CStringView(const CStringView&) noexcept = default;
	constexpr CStringView(CStringView&&) noexcept = default;
	constexpr CStringView& operator=(const CStringView&) noexcept = default;
	constexpr CStringView& operator=(CStringView&&) noexcept = default;

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

// comparison operators
#define COMPARE_BUFF_STRVIEW(oper) template<size_t N> \
	constexpr bool operator oper (const StringBuff<N>& a, const StringView& b) \
		noexcept \
	{ \
		return (StringView(a) oper b); \
	}

#define COMPARE_STRVIEW_BUFF(oper) template<size_t N> \
	constexpr bool operator oper (const StringView& a, const StringBuff<N>& b) \
		noexcept \
	{ \
		return (a oper StringView(b)); \
	}

COMPARE_BUFF_STRVIEW(==)
COMPARE_BUFF_STRVIEW(!=)
COMPARE_BUFF_STRVIEW(<)
COMPARE_BUFF_STRVIEW(>)
COMPARE_BUFF_STRVIEW(<=)
COMPARE_BUFF_STRVIEW(>=)

COMPARE_STRVIEW_BUFF(==)
COMPARE_STRVIEW_BUFF(!=)
COMPARE_STRVIEW_BUFF(<)
COMPARE_STRVIEW_BUFF(>)
COMPARE_STRVIEW_BUFF(<=)
COMPARE_STRVIEW_BUFF(>=)

#undef COMPARE_BUFF_STRVIEW
#undef COMPARE_STRVIEW_BUFF

constexpr inline StringView substring(const StringView& str,
	StringView::size_type beg, StringView::size_type end = StringView::npos)
{
	return str.substring(beg, end);
}

template<class CharT, class Traits, class Char>
constexpr std::basic_ostream<CharT, Traits>& operator<<(
	std::basic_ostream<CharT, Traits>& os, const StringBase<Char>& s)
{
	return os.write(s.data(), s.size());
}

// Compares two StringView, but before comparing two characters modifies them
// with f()
template<class Func>
constexpr bool special_less(const StringView& a, const StringView& b, Func&& f)
{
	size_t len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
		if (f(a[i]) != f(b[i]))
			return f(a[i]) < f(b[i]);

	return b.size() > len;
}

// Checks whether two StringView are equal, but before comparing two characters
// modifies them with f()
template<class Func>
constexpr bool special_equal(const StringView& a, const StringView& b, Func&& f)
{
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i)
		if (f(a[i]) != f(b[i]))
			return  false;

	return true;
}

// Checks whether lowered @p a is equal to lowered @p b
constexpr inline bool lower_equal(const StringView& a, const StringView& b)
	noexcept
{
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

// Only to use with standard integral types
template<class T, size_t N = meta::ToString<std::numeric_limits<T>::max()>
	::arr_value.size() + 2> // +1 for a terminating null char and +1 for the
	                        // minus sign
StringBuff<N> toString(T x) {
	static_assert(N >= 2, "Needed to at least return \"0\"");
	if (x == 0)
		return {1, '0'};

	StringBuff<N> buff;
	if (std::is_signed<T>::value && x < 0) {
		while (x) {
			T x2 = x / 10;
			buff[buff.len++] = x2 * 10 - x + '0';
			x = x2;
		}
		buff[buff.len++] = '-';

		std::reverse(buff.str, buff.str + buff.len);
		return buff;
	}

	while (x) {
		T x2 = x / 10;
		buff[buff.len++] = x - x2 * 10 + '0';
		x = x2;
	}

	std::reverse(buff.str, buff.str + buff.len);
	return buff;
}

// Converts T to std::string
template<class T>
typename std::enable_if<!std::is_integral<T>::value, std::string>::type
	toString(T x)
{
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
inline auto toStr(Args&&... args) {
	return toString(std::forward<Args>(args)...);
}

// Like strtou() but places the result number into x
inline int strToNum(std::string& x, const StringView& s) {
	for (char c : s)
		if (!isdigit(c))
			return -1;

	x = s.to_string();
	return s.size();
}

// Like strToNum() but ends on the first occurrence of @p c or @p s's end
inline int strToNum(std::string& x, const StringView& s, char c) {
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
constexpr inline size_t find(const StringView& str, char c, size_t beg = 0) {
	return str.find(c, beg);
}

// Like string::find() but searches in [beg, end)
constexpr inline size_t find(const StringView& str, char c, size_t beg,
	size_t end)
{
	return str.find(c, beg, end);
}

// Compares two strings: @p str[beg, end) and @p s
constexpr inline int compare(const StringView& str, size_t beg, size_t end,
	const StringView& s) noexcept
{
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

// Compares @p str[pos, str.find(c, pos)) and @p s
constexpr inline int compareTo(const StringView& str, size_t pos, char c,
	const StringView& s) noexcept
{
	return compare(str, pos, str.find(c, pos), s);
}

// Returns true if str1[0...len-1] == str2[0...len-1], false otherwise
// Comparison always takes O(len) iterations (that is why it is slow);
// it applies to security
bool slowEqual(const char* str1, const char* str2, size_t len) noexcept;

inline bool slowEqual(const StringView& str1, const StringView& str2)
	noexcept
{
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

std::string encodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string decodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

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
inline std::string toHex(const StringView& str) {
	return toHex(str.data(), str.size());
}

constexpr inline bool hasPrefix(const StringView& str, const StringView& prefix)
	noexcept
{
	return (str.compare(0, prefix.size(), prefix) == 0);
}

template<class Iter>
constexpr bool hasPrefixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
constexpr inline bool hasPrefixIn(const StringView& str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

constexpr inline bool hasPrefixIn(const StringView& str,
	const std::initializer_list<StringView>& x) noexcept
{
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

constexpr inline bool hasSuffix(const StringView& str, const StringView& suffix)
	noexcept
{
	return (str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

template<class Iter>
constexpr bool hasSuffixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasSuffix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
constexpr inline bool hasSuffixIn(const StringView& str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasSuffix(str, a))
			return true;

	return false;
}

constexpr inline bool hasSuffixIn(const StringView& str,
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
void appendHtmlEscaped(std::string& str, const StringView& s);

// Escapes HTML unsafe character sequences
template<class T>
inline std::string htmlEscape(T&& s) {
	std::string res;
	appendHtmlEscaped(res, std::forward<T>(s));
	return res;
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

constexpr inline bool isInteger(const StringView& s, size_t beg = 0,
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

constexpr inline bool isDigit(const StringView& s) noexcept {
	if (s.empty())
		return false;

	for (char c : s)
		if (c < '0' || c > '9')
			return false;

	return true;
}

constexpr inline bool isReal(const StringView& s) noexcept {
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
constexpr inline bool isDigitNotGreaterThan(const StringView& s) noexcept {
	constexpr auto x = meta::ToString<MAX_VAL>::arr_value;
	return isDigit(s) && !StrNumCompare()({x.data(), x.size()}, s);
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
 *   Warning! If return value is -1 then *x may not be assigned, so using *x
 *   after unsuccessful call is not safe.
 */
template<class T>
constexpr int strtoi(const StringView& s, T *x, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return -1;

	if (x == nullptr)
		return (isInteger(s, beg, end) ? end - beg : -1);

	bool minus = (s[beg] == '-');
	int res = 0;
	*x = 0;
	if ((s[beg] == '-' || s[beg] == '+') && (++res, ++beg) == end)
			return -1; // sign is not a number
	for (size_t i = beg; i < end; ++i) {
		if (s[i] >= '0' && s[i] <= '9')
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	if (minus)
		*x = -*x;

	return res + end - beg;
}

// Like strtoi() but assumes that @p s is a unsigned integer
template<class T>
constexpr int strtou(const StringView& s, T *x, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return -1;

	if (x == nullptr)
		return (isDigit(s.substring(beg + (s[beg] == '+'), end)) ? end - beg
			: -1);

	int res = 0;
	*x = 0;
	if (s[beg] == '+' && (++res, ++beg) == end)
		return -1; // sign is not a number

	for (size_t i = beg; i < end; ++i) {
		if (s[i] >= '0' && s[i] <= '9')
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	return res + end - beg;
}

// Converts string to long long using strtoi
constexpr inline long long strtoll(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtoi() won't parse all the
	// string, and may (but don't have to) return parsed value
	long long x = 0;
	strtoi(s, &x, beg, end);
	return x;
}

// Converts string to unsigned long long using strtou
constexpr inline unsigned long long strtoull(const StringView& s,
	size_t beg = 0, size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtou() won't parse all the
	// string, and may (but don't have to) return parsed value
	unsigned long long x = 0;
	strtou(s, &x, beg, end);
	return x;
}

// Converts digits @p str to @p T, WARNING: assumes that isDigit(str) == true
template<class T>
constexpr T digitsToU(const StringView& str) noexcept {
	T x = 0;
	for (char c : str)
		x = 10 * x + c - '0';
	return x;
}

/**
 * @brief Converts usec (ULL) to sec (double as string)
 *
 * @param x usec value
 * @param prec precision (maximum number of digits after '.', if greater than 6,
 *   then it is downgraded to 6)
 * @param trim_zeros set whether to trim trailing zeros
 *
 * @return floating-point x in sec as string
 */
template<size_t N = meta::ToString<UINT64_MAX>::arr_value.size() + 2> // +2 for
	// terminating null and decimal point
StringBuff<N> usecToSecStr(uint64_t x, uint prec, bool trim_zeros = true) {
	uint64_t y = x / 1'000'000;
	auto res = toString<uint64_t, N>(y);

	y = x - y * 1'000'000;
	res.raw_append('.');
	for (unsigned i = res.len + 5; i >= res.len; --i) {
		res[i] = '0' + y % 10;
		y /= 10;
	}

	// Truncate trailing zeros
	unsigned i = res.len + std::min(5, int(prec) - 1);
	// i will point to the last character of the result
	if (trim_zeros)
		while (i >= res.len && res[i] == '0')
			--i;

	if (i == res.len - 1)
		res.len = i; // Trim trailing '.'
	else
		res.len = ++i;

	res[i] = '\0';
	return res;
}

template<class T>
constexpr inline auto string_length(const T& x) noexcept -> decltype(x.size()) {
	return x.size();
}

template<class T>
constexpr inline size_t string_length(T* x) noexcept {
	return __builtin_strlen(x);
}

constexpr inline size_t string_length(char) noexcept { return 1; }

template<class T>
constexpr inline auto data(const T& x) noexcept { return x.data(); }

template<class T, size_t N>
constexpr inline const T* data(const T (&x)[N]) noexcept { return x; }

constexpr inline auto data(const char* const x) noexcept { return x; }

constexpr inline auto data(char* const x) noexcept { return x; }

constexpr inline const char* data(const char& c) noexcept { return &c; }

template<class Char>
constexpr inline std::string& operator+=(std::string& str,
	const StringBase<Char>& s)
{
	return str.append(s.data(), s.size());
}

inline std::string& operator+=(std::string& str, const meta::string& s) {
	return str.append(s.data(), s.size());
}

/**
 * @brief Concentrates @p args into std::string
 *
 * @param args string-like objects
 *
 * @return concentration of @p args
 */
template<class... Args>
inline std::string concat(Args&&... args) {
	size_t total_length = 0;
	int foo[] = {(total_length += string_length(args), 0)...};
	(void)foo;

	std::string res;
	res.reserve(total_length);
	int bar[] = {(res += std::forward<Args>(args), 0)...};
	(void)bar;
	return res;
}

enum Adjustment : uint8_t { LEFT, RIGHT };

/**
 * @brief Returns a padded string
 * @details Examples:
 *   paddedString("abc", 5) -> "  abc"
 *   paddedString("abc", 5, false) -> "abc  "
 *   paddedString("1234", 7, true, '0') -> "0001234"
 *   paddedString("1234", 4, true, '0') -> "1234"
 *   paddedString("1234", 2, true, '0') -> "1234"
 *
 * @param s string
 * @param len length of result string
 * @param left whether adjust to left or right
 * @param fill character used to fill blank fields
 *
 * @return formatted string
 */
std::string paddedString(const StringView& s, size_t len,
	Adjustment adj = RIGHT, char fill = ' ');

#define throw_assert(expr) \
	((expr) ? (void)0 : throw std::runtime_error(concat(__FILE__, ':', \
	toStr(__LINE__), ": ", __PRETTY_FUNCTION__, \
	": Assertion `" #expr " failed.")))

/* ============================ IMPLEMENTATION ============================== */
template<size_t N>
inline constexpr StringBuff<N>::StringBuff(unsigned count, char ch) : len(count)
{
	throw_assert(count <= max_size);
	std::fill(str, str + count, ch);
	str[count] = '\0';
}

template<size_t N>
template<class... Args>
inline StringBuff<N>& StringBuff<N>::raw_append(Args&&... args) {
	// Sum length of all args
	size_t final_len = len;
	(void)std::initializer_list<size_t>{(final_len += string_length(args))...};
	throw_assert(final_len <= max_size);

	// Concentrate them into str[]
	auto impl_append = [&](auto&& s) {
		auto sl = string_length(s);
		std::copy(::data(s), ::data(s) + sl, str + len);
		len += sl;
	};
	(void)impl_append; // Ignore warning 'unused' when no arguments are provided
	(void)std::initializer_list<int>{(impl_append(std::forward<Args>(args)), 0)...};

	return *this;
}
