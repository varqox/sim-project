#pragma once

#include "likely.h"
#include "meta.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

#ifdef _GLIBCXX_DEBUG
#  include <cassert>
#endif

template<class Char>
class StringBase {
public:
	// Types
	typedef const Char& const_reference;
	typedef Char& reference;
	typedef Char* pointer;
	typedef const Char* const_pointer;
	typedef const_pointer const_iterator;
	typedef const_iterator iterator;
	typedef size_t size_type;
	static constexpr size_type npos = -1;

protected:
	pointer str;
	size_type len;

public:
	constexpr StringBase() noexcept : str(""), len(0) {}

	template<class CharT, size_t N>
	constexpr StringBase(const CharT(&s)[N]) noexcept : str(s), len(N - 1) {}

	constexpr StringBase(const meta::string& s) noexcept
		: str(s.data()), len(s.size()) {}

	constexpr StringBase(pointer s) noexcept
		: str(s), len(__builtin_strlen(s)) {}

	constexpr StringBase(pointer s, size_type n) noexcept : str(s), len(n) {}

	// Constructs StringView from substring [beg, beg + n) of string s
	StringBase(const std::string& s, size_type beg = 0, size_type n = npos)
		noexcept
		: str(s.data() + std::min(beg, s.size())),
			len(std::min(n, s.size() - std::min(beg, s.size()))) {}

	template<class CharT>
	constexpr StringBase(const StringBase<CharT>& s) noexcept
		: str(s.data()), len(s.size()) {}

	template<class CharT>
	constexpr StringBase(StringBase<CharT>&& s) noexcept
		: str(s.data()), len(s.size()) {}

	template<class CharT>
	StringBase& operator=(StringBase<CharT>&& s) noexcept {
		str = s.data();
		len = s.size();
		return *this;
	}

	template<class CharT>
	StringBase& operator=(const StringBase<CharT>& s) noexcept {
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
	const_reference operator[](size_type n) const noexcept {
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
	const_reference at(size_type n) const {
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
	int compare(const StringBase& s) const noexcept {
		size_type clen = std::min(len, s.len);
		int rc = memcmp(str, s.str, clen);
		return rc != 0 ? rc : (len == s.len ? 0 : ((len < s.len) ? -1 : 1));
	}

	int compare(size_type pos, size_type count, const StringBase& s) const {
		return substr(pos, count).compare(s);
	}

	// Returns position of the first character of the first substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type find(const StringBase& s) const noexcept {
		if (s.len == 0)
			return 0;

		// KMP algorithm
		size_type p[s.len], k = p[0] = 0;
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

	size_type find(const StringBase& s, size_type beg1, size_type endi1) const {
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, const StringBase& s, size_type beg1 = 0) const
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

	size_type find(char c, size_type beg = 0) const noexcept {
		for (; beg < len; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	size_type find(char c, size_type beg, size_type endi) const noexcept {
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
		size_type p[s.len], slen1 = s.len - 1, k = p[slen1] = slen1;
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

	size_type rfind(const StringBase& s, size_type beg1, size_type endi1) const
	{
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, const StringBase& s, size_type beg1 = 0)
		const
	{
		return substr(beg).rfind(s.substr(beg1, len - beg1));
	}

	size_type rfind(size_type beg, const StringBase& s, size_type beg1,
		size_type endi1) const
	{
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, size_type endi, const StringBase& s,
		size_type beg1 = 0) const
	{
		return substr(beg, endi).rfind(s.substr(beg1, len - beg1));
	}

	size_type rfind(size_type beg, size_type endi, const StringBase& s,
		size_type beg1, size_type endi1) const
	{
		return substr(beg, endi).rfind(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(char c, size_type beg = 0) const
		noexcept
	{
		for (size_type endi = len - 1; endi >= beg; --endi)
			if (str[endi] == c)
				return endi;

		return npos;
	}

	size_type rfind(char c, size_type beg, size_type endi) const
		noexcept
	{
		if (endi > len)
			endi = len;

		for (--endi; endi >= beg; --endi)
			if (str[endi] == c)
				return endi;

		return npos;
	}

protected:
	// Returns a StringBase of the substring [pos, ...)
	StringBase substr(size_type pos = 0) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, len - pos);
	}

	// Returns a StringBase of the substring [pos, pos + count)
	StringBase substr(size_type pos, size_type count) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, std::min(count, len - pos));
	}

	// Returns a StringBase of the substring [beg, endi)
	StringBase substring(size_type beg, size_type endi) const {
		if (beg > endi || endi > len)
			throw std::out_of_range("StringBase::substring");

		return StringBase(str + beg, endi - beg);
	}

public:
	std::string to_string() const { return std::string(str, len); }

	// comparison operators
	friend bool operator==(const StringBase& a, const StringBase& b) noexcept {
		return (a.len == b.len && memcmp(a.str, b.str, a.len) == 0);
	}

	friend bool operator!=(const StringBase& a, const StringBase& b) noexcept {
		return (a.len != b.len || memcmp(a.str, b.str, a.len) != 0);
	}

	friend bool operator<(const StringBase& a, const StringBase& b) noexcept {
		return (a.compare(b) < 0);
	}

	friend bool operator>(const StringBase& a, const StringBase& b) noexcept {
		return (a.compare(b) > 0);
	}

	friend bool operator<=(const StringBase& a, const StringBase& b) noexcept {
		return (a.compare(b) <= 0);
	}

	friend bool operator>=(const StringBase& a, const StringBase& b) noexcept {
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

	template<class Char>
	FixedString(const StringBase<Char>& s)
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

	StringView() noexcept : StringBase("", 0) {}

	StringView(const StringView&) noexcept = default;

	StringView(StringView&&) noexcept = default;

	template<class Char>
	StringView(const StringBase<Char>& s) noexcept : StringBase(s) {}

	template<class Char>
	StringView& operator=(const StringBase<Char>& s) noexcept {
		StringBase::operator=(s);
		return *this;
	}

	StringView& operator=(const StringView& s) noexcept = default;

	StringView& operator=(StringView&& s) noexcept = default;

	~StringView() = default;

	void clear() noexcept { len = 0; }

	// Removes prefix of length n
	void removePrefix(size_type n) noexcept {
		if (n > len)
			n = len;
		str += n;
		len -= n;
	}

	// Removes suffix of length n
	void removeSuffix(size_type n) noexcept {
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
	void removeLeading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}
		str += i;
		len -= i;
	}

	void removeLeading(char c) noexcept {
		removeLeading([c](char x) { return (x == c); });
	}

	// Removes trailing characters for which f() returns true
	template<class Func>
	void removeTrailing(Func&& f) {
		while (len > 0 && f(back()))
			--len;
	}

	void removeTrailing(char c) noexcept {
		removeTrailing([c](char x) { return (x == c); });
	}

	// Extracts leading characters for which f() returns true
	template<class Func>
	StringView extractLeading(Func&& f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}

		StringView res = substring(0, i);
		str += i;
		len -= i;

		return res;
	}

	// Extracts trailing characters for which f() returns true
	template<class Func>
	StringView extractTrailing(Func&& f) {
		size_type i = len;
		for (; i > 0 && f(str[i - 1]); --i) {}

		StringView res = substring(i, len);
		len = i;

		return res;
	}

	using StringBase::substr;
	using StringBase::substring;
};

inline StringView substring(const std::string& str, std::string::size_type beg,
	std::string::size_type end = std::string::npos)
{
	return StringView(str, beg, end - beg);
}

template<class CharT, class Traits, class Char>
std::basic_ostream<CharT, Traits>& operator<<(
	std::basic_ostream<CharT, Traits>& os, const StringBase<Char>& s)
{
	return os.write(s.data(), s.size());
}

// Compares two StringView, but before comparing two characters modifies them
// with f()
template<class Func>
bool special_less(const StringView& a, const StringView& b, Func&& f) {
	size_t len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
		if (f(a[i]) != f(b[i]))
			return f(a[i]) < f(b[i]);

	return b.size() > len;
}

// Checks whether two StringView are equal, but before comparing two characters
// modifies them with f()
template<class Func>
bool special_equal(const StringView& a, const StringView& b, Func&& f) {
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i)
		if (f(a[i]) != f(b[i]))
			return  false;

	return true;
}

// Checks whether lowered @p a is equal to lowered @p b
inline bool lower_equal(const StringView& a, const StringView& b) noexcept {
	return special_equal<int(int)>(a, b, tolower);
}

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	bool operator()(StringView a, StringView b) const {
		a.removeLeading([](char c) { return (c == '0'); });
		b.removeLeading([](char c) { return (c == '0'); });
		return (a.size() == b.size() ? a < b : a.size() < b.size());
	}
};

// Only to use with standard integral types
template<class T>
typename std::enable_if<std::is_integral<T>::value, std::string>::type
	toString(T x)
{
	if (x == 0)
		return std::string(1, '0');

	static_assert(ULLONG_MAX <= 18446744073709551615ull, "Enlarge the array!");
	std::array<char, 21> buff;
	unsigned i = buff.size();

	if (std::is_signed<T>::value && x < 0) {
		while (x) {
			T x2 = x / 10;
			buff[--i] = x2 * 10 - x + '0';
			x = x2;
		}
		buff[--i] = '-';
		return std::string(buff.data() + i, buff.size() - i);
	}

	while (x) {
		T x2 = x / 10;
		buff[--i] = x - x2 * 10 + '0';
		x = x2;
	}
	return std::string(buff.data() + i, buff.size() - i);
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
inline std::string toStr(Args&&... args) {
	return toString(std::forward<Args>(args)...);
}

// Like strtou() but places number into x
int strToNum(std::string& x, const StringView& s, size_t beg = 0,
	size_t end = StringView::npos);

// Like strToNum() but ends on first occurrence of @p c or @p s end
int strToNum(std::string& x, const StringView& s, size_t beg, char c);

// Like string::find() but searches in [beg, end)
inline size_t find(const StringView& str, char c, size_t beg = 0) {
	return str.find(c, beg);
}

// Like string::find() but searches in [beg, end)
inline size_t find(const StringView& str, char c, size_t beg, size_t end) {
	return str.find(c, beg, end);
}

// Compares two strings: @p str[beg, end) and @p s
inline int compare(const StringView& str, size_t beg, size_t end,
	const StringView& s) noexcept
{
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

// Compares @p str[pos, str.find(c, pos)) and @p s
inline int compareTo(const StringView& str, size_t pos, char c,
	const StringView& s) noexcept
{
	return compare(str, pos, str.find(c, pos), s);
}

// Returns true if str1[0...len-1] == str2[0...len-1], false otherwise
// Comparison always takes O(len) iterations (that is why it is slow);
// it applies to security
bool slowEqual(const char* str1, const char* str2, size_t len) noexcept;

inline bool slowEqual(const std::string& str1, const std::string& str2)
	noexcept
{
	return slowEqual(str1.data(), str2.data(),
		std::min(str1.size(), str2.size())) && str1.size() == str2.size();
}

// Returns @p str without trailing characters for which f() returns true
template<class Func>
std::string withoutTrailing(const std::string& str, Func&& f) {
	auto len = str.size();
	while (len > 0 && f(str[len - 1]))
		--len;
	return std::string(str, 0, len);
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

// Returns @p str without trailing @p c
inline std::string withoutTrailing(const std::string& str, char c) {
	return withoutTrailing(str, [c](char x) { return (x == c); });
}

// Removes trailing @p c
inline void removeTrailing(std::string& str, char c) noexcept {
	removeTrailing(str, [c](char x) { return (x == c); });
}

std::string encodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string decodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string tolower(std::string str);

inline int hextodec(int c) noexcept {
	c = ::tolower(c);
	return (c >= 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectoHex(int x) noexcept { return x > 9 ? 'A' - 10 + x : x + '0'; }

inline char dectohex(int x) noexcept { return x > 9 ? 'a' - 10 + x : x + '0'; }

// Converts each byte of @p str to two hex digits using dectohex()
std::string toHex(const char* str, size_t len);

// Converts each byte of @p str to two hex digits using dectohex()
inline std::string toHex(const std::string& str) {
	return toHex(str.data(), str.size());
}

inline bool hasPrefix(const StringView& str, const StringView& prefix) noexcept
{
	return (str.compare(0, prefix.size(), prefix) == 0);
}

template<class Iter>
bool hasPrefixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
inline bool hasPrefixIn(const StringView& str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

inline bool hasPrefixIn(const StringView& str,
	const std::initializer_list<StringView>& x) noexcept
{
	for (auto&& a : x)
		if (hasPrefix(str, a))
			return true;

	return false;
}

inline bool hasSuffix(const StringView& str, const StringView& suffix) noexcept
{
	return (str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

template<class Iter>
bool hasSuffixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (hasSuffix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
inline bool hasSuffixIn(const StringView& str, T&& x) noexcept {
	for (auto&& a : x)
		if (hasSuffix(str, a))
			return true;

	return false;
}

inline bool hasSuffixIn(const StringView& str,
	const std::initializer_list<StringView>& x) noexcept
{
	for (auto&& a : x)
		if (hasSuffix(str, a))
			return true;

	return false;
}

// Escapes HTML unsafe character and appends it to @p str
void htmlSpecialChars(std::string& str, char c);

// Escapes HTML unsafe character sequences and appends them to @p str
void htmlSpecialChars(std::string& str, const StringView& s);

// Escapes HTML unsafe character sequences
template<class T>
inline std::string htmlSpecialChars(T&& s) {
	std::string res;
	htmlSpecialChars(res, std::forward<T>(s));
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
bool isInteger(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos) noexcept;

// Checks whether string @p s[beg, end) consist only of digits
bool isDigit(const StringView& s, size_t beg, size_t end = StringView::npos)
	noexcept;

/// Checks whether string @p s consist only of digits
inline bool isDigit(const StringView& s) noexcept { return isDigit(s, 0); }

/// Checks whether string @p s consist only of digits and is not greater than
/// @p MAX_VAL
template<uintmax_t MAX_VAL>
inline bool isDigitNotGreaterThan(const StringView& s) noexcept {
	constexpr auto x = meta::ToString<MAX_VAL>::arr_value;
	return isDigit(s, 0) && !StrNumCompare()({x.data(), x.size()}, s);
}

bool isReal(const StringView& s, size_t beg, size_t end = StringView::npos)
	noexcept;

inline bool isReal(const StringView& s) noexcept { return isReal(s, 0); }

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
int strtoi(const StringView& s, T *x, size_t beg = 0,
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
		if (isdigit(s[i]))
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
int strtou(const StringView& s, T *x, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return -1;

	if (x == nullptr)
		return (isDigit(s, beg + (s[beg] == '+'), end) ? end - beg : -1);

	int res = 0;
	*x = 0;
	if (s[beg] == '+' && (++res, ++beg) == end)
		return -1; // sign is not a number

	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	return res + end - beg;
}

// Converts string to long long using strtoi
inline long long strtoll(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtoi() won't parse all the
	// string, and may (but don't have to) return parsed value
	long long x = 0;
	strtoi(s, &x, beg, end);
	return x;
}

// Converts string to unsigned long long using strtou
inline unsigned long long strtoull(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos) noexcept
{
	// TODO: when argument is not a valid number, strtou() won't parse all the
	// string, and may (but don't have to) return parsed value
	unsigned long long x = 0;
	strtou(s, &x, beg, end);
	return x;
}

// Converts digits @p str to @p T, WARNING: assumes that isDigit(str) == true
template<class T>
T digitsToU(const StringView& str) noexcept {
	T x = 0;
	for (char c : str)
		x = 10 * x + c - '0';
	return x;
}

/**
 * @brief Converts usec (ULL) to sec (double as string)
 *
 * @param x usec value
 * @param prec precision (maximum number of digits after '.')
 * @param trim_zeros set whether to trim trailing zeros
 *
 * @return floating-point x in sec as string
 */
std::string usecToSecStr(uint64_t x, uint prec, bool trim_zeros = true);

template<class T>
inline size_t string_length(const T& x) noexcept { return x.size(); }

template<class T>
inline size_t string_length(T* x) noexcept { return __builtin_strlen(x); }

inline size_t string_length(char) noexcept { return 1; }

template<class Char>
inline std::string& operator+=(std::string& str, const StringBase<Char>& s) {
	str.append(s.data(), s.size());
	return str;
}

inline std::string& operator+=(std::string& str, const meta::string& s) {
	str.append(s.data(), s.size());
	return str;
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
 * @brief Returns widened string
 * @details Examples:
 *   widedString("abc", 5) -> "  abc"
 *   widedString("abc", 5, false) -> "abc  "
 *   widedString("1234", 7, true, '0') -> "0001234"
 *   widedString("1234", 4, true, '0') -> "1234"
 *   widedString("1234", 2, true, '0') -> "1234"
 *
 * @param s string
 * @param len length of result string
 * @param left whether adjust to left or right
 * @param fill character used to fill blank fields
 *
 * @return formatted string
 */
std::string widedString(const StringView& s, size_t len, Adjustment adj = RIGHT,
	char fill = ' ');
