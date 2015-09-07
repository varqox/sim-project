#pragma once

#include <cstring>
#include <stdexcept>

class StringView {
public:
	// Types
	typedef const char& const_reference;
	typedef const char& reference;
	typedef const char* pointer;
	typedef const char* const_pointer;
	typedef const_pointer const_iterator;
	typedef const_iterator iterator;
	typedef size_t size_type;
	static const size_type npos = -1;

private:
	pointer str;
	size_type len;

public:
	StringView(const char* s = "") : str(s), len(strlen(s)) {}

	StringView(const char* s, size_type n) : str(s), len(n) {}

	StringView(const StringView& s) : str(s.str), len(s.len) {}

	StringView(const std::string& s, size_type beg = 0, size_type endi = npos)
		: str(s.data() + std::min(beg, s.size())),
		len(std::min(s.size(), endi)) {}

	~StringView() {}

	StringView& operator=(const StringView& s) {
		str = s.str;
		len = s.len;
		return *this;
	}

	// Returns whether the StringView is empty (size() == 0)
	bool empty() const { return len == 0; }

	// Returns the number of characters in the StringView
	size_type size() const { return len; }

	// Returns the number of characters in the StringView
	size_type length() const { return len; }

	// Returns a pointer to the underlying character array
	const_pointer data() const { return str; }

	const_iterator begin() const { return str; }

	const_iterator cbegin() const { return str; }

	const_iterator end() const { return str + len; }

	const_iterator cend() const { return str + len; }

	// Returns reference to first element
	const_reference front() const { return str[0]; }

	// Returns reference to last element
	const_reference back() const { return str[len - 1]; }

	// Returns reference to n-th element
	const_reference operator[](size_type n) const { return str[n]; }

	// Like operator[] but throws exception if n >= size()
	const_reference at(size_type n) const {
		if (n >= len)
			throw std::out_of_range("StringView::at");

		return str[n];
	}

	// Swaps two StringView
	void swap(StringView& s) {
		// Swap str
		pointer p = str;
		str = s.str;
		s.str = p;
		// Swap len
		size_type x = len;
		len = s.len;
		s.len = x;
	}

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

	/**
	 * @brief Compares two StringView
	 *
	 * @param s StringView to compare to
	 * @return -1 - this < @p s, 0 - equal, 1 - this > @p s
	 */
	int compare(const StringView& s) const {
		size_type clen = std::min(len, s.len);
		int rc = strncmp(str, s.str, clen);
		return rc != 0 ? rc : (len == s.len ? 0 : (len < s.len) ? -1 : 1);
	}

	int compare(size_type pos, size_type count, const StringView& s) const {
		return substr(pos, count).compare(s);
	}

	// Returns position of the first character of the first substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type find(const StringView& s) const;

	size_type find(const StringView& s, size_type beg1,
			size_type endi1 = npos) const {
		return find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, const StringView& s, size_type beg1 = 0,
			size_type endi1 = npos) const {
		return substr(beg).find(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(size_type beg, size_type endi, const StringView& s,
			size_type beg1 = 0, size_type endi1 = npos) const {
		return substr(beg, endi).find(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type find(char c, size_type beg = 0, size_type endi = npos) const;

	// Returns position of the first character of the last substring equal to
	// the given character sequence, or npos if no such substring is found
	size_type rfind(const StringView& s) const;

	size_type rfind(const StringView& s, size_type beg1,
			size_type endi1 = npos) const {
		return rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, const StringView& s, size_type beg1 = 0,
			size_type endi1 = npos) const {
		return substr(beg).rfind(s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(size_type beg, size_type endi, const StringView& s,
			size_type beg1 = 0, size_type endi1 = npos) const {
		return substr(beg, endi).rfind(
			s.substr(beg1, std::min(endi1, len) - beg1));
	}

	size_type rfind(char c, size_type beg = 0, size_type endi = npos) const;

	// Returns a StringView of the substring [pos, pos + count)
	StringView substr(size_type pos = 0, size_type count = npos) const {
		if (pos > len)
			throw std::out_of_range("StringView::substr");

		return StringView(str + pos, std::min(count, len - pos));
	}

	std::string to_string() const { return std::string(str, str + len); }

	// comparison operators
	friend bool operator==(const StringView& a, const StringView& b) {
		return a.len == b.len && strncmp(a.str, b.str, a.len) == 0;
	}

	friend bool operator!=(const StringView& a, const StringView& b) {
		return a.len != b.len || strncmp(a.str, b.str, a.len) != 0;
	}

	friend bool operator<(const StringView& a, const StringView& b) {
		return a.compare(b) < 0;
	}

	friend bool operator>(const StringView& a, const StringView& b) {
		return a.compare(b) > 0;
	}

	friend bool operator<=(const StringView& a, const StringView& b) {
		return a.compare(b) <= 0;
	}

	friend bool operator>=(const StringView& a, const StringView& b) {
		return a.compare(b) >= 0;
	}
};

template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const StringView& s) {
	return os.write(s.data(), s.size());
}

// Converts long long to std::string
std::string toString(long long a);

// Converts unsigned long long to std::string
std::string toString(unsigned long long a);

// Converts size_t to std::string
inline std::string toString(size_t a) {
	return toString(static_cast<unsigned long long>(a));
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

std::string encodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string decodeURI(const StringView& str, size_t beg = 0,
	size_t end = StringView::npos);

std::string tolower(std::string str);

inline int hextodec(int c) {
	c = tolower(c);
	return (c >= 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectohex(int x) { return x > 9 ? 'A' + x - 10 : x + '0'; }

inline bool isPrefix(const StringView& str, const StringView& prefix) {
	return str.compare(0, prefix.size(), prefix) == 0;
}

template<class Iter>
bool isPrefixIn(const StringView& str, Iter beg, Iter end) {
	while (beg != end) {
		if (isPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

inline bool isSuffix(const StringView& str, const StringView& suffix) {
	return str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

template<class Iter>
bool isSuffixIn(const StringView& str, Iter beg, Iter end) {
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
 * Notes:
 * - empty string is not an integer
 * - sign is not an integer
 *
 * @param s string
 * @param beg beginning
 * @param end position of first character not to check
 * @return result - whether string @p s[beg, end) is an integer
 */
bool isInteger(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos);

// Checks whether string @p s[beg, end) consist only of digits
bool isDigit(const StringView& s, size_t beg, size_t end = StringView::npos);

// Checks whether string @p s consist only of digits
inline bool isDigit(const StringView& s) { return isDigit(s, 0); }

bool isReal(const StringView& s, size_t beg = 0,
	size_t end = StringView::npos);

/* Converts s: [beg, end) to size_t
*  if end > s.size() or enStringViewnpos then end = s.size()
*  if beg > end then beg = end
*  if x == NULL then only validate
*  Return value: -1 if s: [beg, end) is not a number
*    otherwise return the number of characters parsed
*/
template<class T>
int strtoi(const StringView& s, T *x, size_t beg = 0,
		size_t end = StringView::npos) {
	if (end > s.size())
		end = s.size();
	if (beg >= end) {
		*x = 0;
		return 0;
	}

	if (x == NULL)
		return isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	bool minus = false;
	if (s[beg] == '+')
		++beg;
	else if (s[beg] == '-') {
		minus = true;
		++beg;
	}
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

// Like strtoi() by assumes that @p s is unsigned integer
template<class T>
int strtou(const StringView& s, T *x, size_t beg = 0,
		size_t end = StringView::npos) {
	if (end > s.size())
		end = s.size();
	if (beg >= end) {
		*x = 0;
		return 0;
	}

	if (x == NULL)
		return s[beg] != '-' && isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	if (s[beg] == '+')
		++beg;

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

/**
 * @brief Converts usec (ULL) to sec (double as string)
 *
 * @param x usec value
 * @param prec precision (maximum number of digits after '.')
 * @param trim_nulls set whether to trim trailing nulls
 * @return floating-point x in sec as string
 */
std::string usecToSecStr(unsigned long long x, unsigned prec,
	bool trim_nulls = true);

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	bool operator()(const StringView& a, const StringView& b) const {
		return a.size() == b.size() ? a < b : a.size() < b.size();
	}
};
