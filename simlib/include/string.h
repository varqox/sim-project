#pragma once

#include "likely.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

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
	StringBase(pointer s = "") : str(s), len(strlen(s)) {}

	StringBase(pointer s, size_type n) : str(s), len(n) {}


	StringBase(const std::string& s, size_type beg = 0, size_type endi = npos)
		: str(s.data() + std::min(beg, s.size())),
		len(std::min(s.size(), endi)) {}

	template<class CharT>
	StringBase(const StringBase<CharT>& s) : str(s.data()), len(s.size()) {}

	template<class CharT>
	StringBase(StringBase<CharT>&& s) : str(s.data()), len(s.size()) {}

	template<class CharT>
	StringBase& operator=(StringBase<CharT>&& s) {
		str = s.data();
		len = s.size();
	}

	template<class CharT>
	StringBase& operator=(const StringBase<CharT>& s) {
		str = s.data();
		len = s.size();
	}

	virtual ~StringBase() {}

	// Returns whether the StringBase is empty (size() == 0)
	bool empty() const noexcept { return len == 0; }

	// Returns the number of characters in the StringBase
	size_type size() const noexcept { return len; }

	// Returns the number of characters in the StringBase
	size_type length() const noexcept { return len; }

	// Returns a pointer to the underlying character array
	pointer data() noexcept { return str; }

	// Returns a const pointer to the underlying character array
	const_pointer data() const noexcept { return str; }

	iterator begin() noexcept { return str; }

	iterator end() noexcept { return str + len; }

	const_iterator begin() const noexcept { return str; }

	const_iterator end() const noexcept { return str + len; }

	const_iterator cbegin() const noexcept { return str; }

	const_iterator cend() const noexcept { return str + len; }

	// Returns reference to first element
	reference front() { return str[0]; }

	// Returns const_reference to first element
	const_reference front() const { return str[0]; }

	// Returns reference to last element
	reference back() { return str[len - 1]; }

	// Returns const_reference to last element
	const_reference back() const { return str[len - 1]; }

	// Returns reference to n-th element
	reference operator[](size_type n) { return str[n]; }

	// Returns const_reference to n-th element
	const_reference operator[](size_type n) const { return str[n]; }

	// Like operator[] but throws exception if n >= size()
	reference at(size_type n) {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

		return str[n];
	}

	// Like operator[] but throws exception if n >= size()
	const_reference at(size_type n) const {
		if (n >= len)
			throw std::out_of_range("StringBase::at");

		return str[n];
	}

	// Swaps two StringBase
	virtual void swap(StringBase& s) {
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
	 * @return -1 - this < @p s, 0 - equal, 1 - this > @p s
	 */
	int compare(const StringBase& s) const {
		size_type clen = std::min(len, s.len);
		int rc = strncmp(str, s.str, clen);
		return rc != 0 ? rc : (len == s.len ? 0 : ((len < s.len) ? -1 : 1));
	}

	int compare(size_type pos, size_type count, const StringBase& s) const {
		return substr(pos, count).compare(s);
	}

	// Returns position of the first character of the first substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type find(const StringBase& s) const {
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

	size_type find(const StringBase& s, size_type beg1,
			size_type endi1 = npos) const {
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, const StringBase& s, size_type beg1 = 0,
			size_type endi1 = npos) const {
		return substr(beg).find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, size_type endi, const StringBase& s,
			size_type beg1 = 0, size_type endi1 = npos) const {
		return substr(beg, endi).find(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(char c, size_type beg = 0, size_type endi = npos) const {
		if (endi > len)
			endi = len;

		for (; beg < endi; ++beg)
			if (str[beg] == c)
				return beg;

		return npos;
	}

	// Returns position of the first character of the last substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type rfind(const StringBase& s) const {
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

	size_type rfind(const StringBase& s, size_type beg1,
			size_type endi1 = npos) const {
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, const StringBase& s, size_type beg1 = 0,
			size_type endi1 = npos) const {
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, size_type endi, const StringBase& s,
			size_type beg1 = 0, size_type endi1 = npos) const {
		return substr(beg, endi).rfind(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(char c, size_type beg = 0, size_type endi = npos) const {
		if (endi > len)
			endi = len;

		for (--endi; endi >= beg; --endi)
			if (str[endi] == c)
				return endi;

		return npos;
	}

protected:
	// Returns a StringBase of the substring [pos, pos + count)
	StringBase substr(size_type pos = 0, size_type count = npos) const {
		if (pos > len)
			throw std::out_of_range("StringBase::substr");

		return StringBase(str + pos, std::min(count, len - pos));
	}


public:
	std::string to_string() const { return std::string(str, str + len); }

	// comparison operators
	friend bool operator==(const StringBase& a, const StringBase& b) {
		return a.len == b.len && strncmp(a.str, b.str, a.len) == 0;
	}

	friend bool operator!=(const StringBase& a, const StringBase& b) {
		return a.len != b.len || strncmp(a.str, b.str, a.len) != 0;
	}

	friend bool operator<(const StringBase& a, const StringBase& b) {
		return a.compare(b) < 0;
	}

	friend bool operator>(const StringBase& a, const StringBase& b) {
		return a.compare(b) > 0;
	}

	friend bool operator<=(const StringBase& a, const StringBase& b) {
		return a.compare(b) <= 0;
	}

	friend bool operator>=(const StringBase& a, const StringBase& b) {
		return a.compare(b) >= 0;
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
	FixedString(const_pointer s) : FixedString(s, strlen(s)) {}

	FixedString(const std::string& s, size_type beg = 0, size_type endi = npos)
		: FixedString(s.data() + std::min(s.size(), beg),
			std::min(s.size(), endi)) {}

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

	// Returns a FixedString of the substring [pos, pos + count)
	FixedString substr(size_type pos = 0, size_type count = npos) const {
		if (pos > len)
			throw std::out_of_range("FixedString::substr");

		return FixedString(str + pos, std::min(count, len - pos));
	}
};

class StringView : public StringBase<const char> {
public:
	using StringBase::StringBase;

	StringView() : StringBase("", 0) {}

	template<class Char>
	StringView(const StringBase<Char>& s) : StringBase(s) {}

	~StringView() = default;

	void clear() { len = 0; }

	// Removes prefix of length n
	void removePrefix(size_type n) {
		if (n > len)
			n = len;
		str += n;
		len -= n;
	}

	// Removes suffix of length n
	void removeSuffix(size_type n) {
		if (n > len)
			len = 0;
		else
			len -= n;
	}

	// Removes trailing characters for which f() returns true
	template<class F>
	void removeLeading(F f) {
		size_type i = 0;
		for (; i < len && f(str[i]); ++i) {}
		str += i;
		len -= i;
	}

	// Removes trailing characters for which f() returns true
	template<class F>
	void removeTrailing(F f) {
		while (len > 0 && f(back()))
			--len;
	}

	using StringBase::substr;
};

template<class CharT, class Traits, class Char>
std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const StringBase<Char>& s) {
	return os.write(s.data(), s.size());
}

// Compares two StringView, but before comparing two characters modifies them
// with f()
template<class F>
bool special_less(const StringView& a, const StringView& b, F f) {
	size_t len = std::min(a.size(), b.size());
	for (size_t i = 0; i < len; ++i)
		if (f(a[i]) != f(b[i]))
			return f(a[i]) < f(b[i]);

	return b.size() > len;
}

// Checks whether two StringView are equal, but before comparing two characters
// modifies them with f()
template<class F>
bool special_equal(const StringView& a, const StringView& b, F f) {
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i)
		if (f(a[i]) != f(b[i]))
			return  false;

	return true;
}

// Checks whether lowered @p a is equal to lowered @p b
inline bool lower_equal(const StringView& a, const StringView& b) {
	return special_equal<int(int)>(a, b, tolower);
}

// Converts T to std::string
template<class T>
std::string toString(T x) {
	static_assert(std::is_integral<T>::value, "template argument not an "
		"integral type");

	std::string res;
	bool minus = false;

	if (std::is_signed<T>::value && x < T()) {
		minus = true;
		x = -x;
	}

	while (x > 0) {
		res += static_cast<char>(x % 10 + '0');
		x /= 10;
	}

	if (res.empty()) {
		res = '0';
	} else {
		if (minus)
			res += '-';
		std::reverse(res.begin(), res.end());
	}

	return res;
}

// TODO
inline std::string toString(double x, int precision = 6) {
	std::array<char, 300> buff;
	int rc = snprintf(buff.data(), buff.size(), "%.*lf", precision, x);
	if (0 < rc && rc < static_cast<int>(buff.size()))
		return std::string(buff.data(), rc);

	return std::to_string(x);
}

// Like strtou() but places number into x
int strToNum(std::string& x, const StringView& s, size_t beg = 0,
	size_t end = StringView::npos);

// Like strToNum() but ends on first occurrence of @p c or @p s end
int strToNum(std::string& x, const StringView& s, size_t beg, char c);

// Like string::find() but searches in [beg, end)
inline size_t find(const StringView& str, char c, size_t beg = 0,
	size_t end = StringView::npos) { return str.find(c, beg, end); }

// Compares two strings: @p str[beg, end) and @p s
inline int compare(const StringView& str, size_t beg, size_t end,
		const StringView& s) {
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

// Compares @p str[pos, str.find(c, pos)) and @p s
inline int compareTo(const StringView& str, size_t pos, char c,
		const StringView& s) {
	return compare(str, pos, str.find(c, pos), s);
}

// Returns true if str1[0...len-1] == str2[0...len-1], false otherwise
// Comparison always takes O(len) iterations (that is why it is slow);
// it applies to security
bool slowEqual(const char* str1, const char* str2, size_t len) noexcept;

inline bool slowEqual(const std::string& str1, const std::string& str2)
		noexcept {
	return slowEqual(str1.data(), str2.data(),
		std::min(str1.size(), str2.size())) && str1.size() == str2.size();
}

std::string withoutTrailing(const std::string& str, char c);

void removeTrailing(std::string& str, char c) noexcept;

template<class Func>
std::string withoutTrailing(const std::string& str, Func f) {
	auto len = str.size();
	while (len > 0 && f(str[len - 1]))
		--len;
	return std::string(str, 0, len);
}

template<class Func>
void removeTrailing(std::string& str, Func f) {
	auto it = str.end();
	while (it != str.begin())
		if (!f(*--it)) {
			++it;
			break;
		}
	str.erase(it, str.end());
}

std::string encodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string decodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string tolower(std::string str);

inline int hextodec(int c) {
	c = tolower(c);
	return (c >= 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectohex(int x) { return x > 9 ? 'A' - 10 + x : x + '0'; }

inline char dectohex2(int x) { return x > 9 ? 'a' - 10 + x : x + '0'; }

// Converts each byte of @p str to two hex digits using dectohex2()
std::string toHex(const char* str, size_t len) noexcept(false);

// Converts each byte of @p str to two hex digits using dectohex2()
inline std::string toHex(const std::string& str) noexcept(false) {
	return toHex(str.data(), str.size());
}

inline bool isPrefix(const StringView& str, const StringView& prefix) noexcept {
	try {
		return str.compare(0, prefix.size(), prefix) == 0;
	} catch (const std::out_of_range& e) {
		// This is a bug (ignore it; Coverity has a problem with that...)
		return false;
	}
}

template<class Iter>
bool isPrefixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (isPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

template<class T>
inline bool isPrefixIn(const StringView& str, const T& x) noexcept {
	return isPrefixIn(str, x.begin(), x.end());
}

inline bool isPrefixIn(const StringView& str,
	const std::initializer_list<StringView>& x) noexcept
{
	return isPrefixIn(str, x.begin(), x.end());
}

inline bool isSuffix(const StringView& str, const StringView& suffix) noexcept {
	try {
		return str.size() >= suffix.size() &&
			str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
	} catch (const std::out_of_range& e) {
		// This is a bug (ignore it; Coverity has a problem with that...)
		return false;
	}
}

template<class Iter>
bool isSuffixIn(const StringView& str, Iter beg, Iter end) noexcept {
	while (beg != end) {
		if (isSuffix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

std::string htmlSpecialChars(const StringView& s);

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
	size_t end = StringView::npos);

// Checks whether string @p s[beg, end) consist only of digits
bool isDigit(const StringView& s, size_t beg, size_t end = StringView::npos);

// Checks whether string @p s consist only of digits
inline bool isDigit(const StringView& s) { return isDigit(s, 0); }

bool isReal(const StringView& s, size_t beg, size_t end = StringView::npos);

inline bool isReal(const StringView& s) { return isReal(s, 0); }

/* Converts s: [beg, end) to size_t
*  if end > s.size() or enStringViewnpos then end = s.size()
*  if beg > end then beg = end
*  if x == nullptr then only validate
*  Return value: -1 if s: [beg, end) is not a number
*    otherwise return the number of characters parsed
*/
template<class T>
int strtoi(const StringView& s, T *x, size_t beg = 0,
		size_t end = StringView::npos) {
	static_assert(std::is_integral<T>::value, "template argument not an "
		"integral type");

	if (end > s.size())
		end = s.size();
	if (beg >= end) {
		*x = 0;
		return 0;
	}

	if (x == nullptr)
		return isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	bool minus = (s[beg] == '-');
	if ((s[beg] == '-' || s[beg] == '+') && ++beg == end)
			return -1; // sign is not a number

	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}
	if (minus)
		*x = -*x;

	return end - beg;
}

// Like strtoi() but assumes that @p s is unsigned integer
template<class T>
int strtou(const StringView& s, T *x, size_t beg = 0,
		size_t end = StringView::npos) {
	static_assert(std::is_integral<T>::value, "template argument not an "
		"integral type");

	if (end > s.size())
		end = s.size();
	if (beg >= end) {
		*x = 0;
		return 0;
	}

	if (x == nullptr)
		return s[beg] != '-' && isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	if (s[beg] == '+' && ++beg == end)
		return -1; // sign is not a number

	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	return end - beg;
}

// Converts string to long long
inline long long strtoll(const StringView& s, size_t beg = 0,
		size_t end = StringView::npos) {
	long long x;
	strtoi(s, &x, beg, end);
	return x;
}

// Converts string to unsigned long long
inline unsigned long long strtoull(const StringView& s, size_t beg = 0,
		size_t end = StringView::npos) {
	unsigned long long x;
	strtou(s, &x, beg, end);
	return x;
}

// Converts digits @p str to @p T, WARNING: assumes that isDigit(str) == true
template<class T>
T digitsToU(const StringView& str) {
	static_assert(std::is_integral<T>::value, "template argument not an "
		"integral type");
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
 * @param trim_nulls set whether to trim trailing nulls
 *
 * @return floating-point x in sec as string
 */
std::string usecToSecStr(unsigned long long x, unsigned prec,
	bool trim_nulls = true);

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	bool operator()(StringView a, StringView b) const {
		a.removeLeading([](char c) { return c == '0'; });
		b.removeLeading([](char c) { return c == '0'; });
		return a.size() == b.size() ? std::string(a.data(), a.size()) < std::string(b.data(), b.size()) : a.size() < b.size();
	}
};

template<class T>
inline size_t string_length(const T& x) { return x.length(); }

template<class T>
inline size_t string_length(T* x) { return strlen(x); }

inline size_t string_length(char) { return 1; }

inline std::string& operator+=(std::string& str, const StringView& s) {
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
template<class... T>
inline std::string concat(const T&... args) {
	size_t total_length = 0;
	int foo[] = {(total_length += string_length(args), 0)...};
	(void)foo;

	std::string res;
	res.reserve(total_length);
	int bar[] = {(res += args, 0)...};
	(void)bar;
	return res;
}

enum Adjustment : uint8_t { LEFT, RIGHT };

/**
 * @brief Returns wided string
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
