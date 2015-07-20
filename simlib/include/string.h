#pragma once

#include <cctype>
#include <string>

std::string toString(long long a);

std::string toString(unsigned long long a);

inline std::string toString(size_t a) {
	return toString(static_cast<unsigned long long>(a));
}

// Like strtou() but places number into x
int strToNum(std::string& x, const std::string& s, size_t beg = 0,
	size_t end = std::string::npos);

// Like strToNum() but ends on first occurrence of @p c or @p s end
int strToNum(std::string& x, const std::string& s, size_t beg, char c);

// Like string::find() but searches in [beg, end)
size_t find(const std::string& str, char c, size_t beg = 0,
	size_t end = std::string::npos);

inline int compare(const std::string& str, size_t beg, size_t end,
		const std::string& s) {
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

inline int compare(const std::string& str, size_t beg, size_t end,
		const char* s) {
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;

	return str.compare(beg, end - beg, s);
}

inline int compareTo(const std::string& str, size_t pos, char c,
		const std::string& s) {
	return compare(str, pos, str.find(c, pos) - pos, s);
}

inline int compareTo(const std::string& str, size_t pos, char c,
		const char* s) {
	return compare(str, pos, str.find(c, pos), s);
}

std::string encodeURI(const std::string& str, size_t beg = 0,
	size_t end = std::string::npos);

std::string decodeURI(const std::string& str, size_t beg = 0,
	size_t end = std::string::npos);

std::string tolower(std::string str);

inline int hextodec(int c) {
	c = tolower(c);
	return (c >= 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectohex(int x) { return x > 9 ? 'A' + x - 10 : x + '0'; }

inline bool isPrefix(const std::string& str, const std::string& prefix) {
	return str.compare(0, prefix.size(), prefix) == 0;
}

template<class Iter>
bool isPrefixIn(const std::string& str, Iter beg, Iter end) {
	while (beg != end) {
		if (isPrefix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

inline bool isSuffix(const std::string& str, const std::string& suffix) {
	return str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

template<class Iter>
bool isSuffixIn(const std::string& str, Iter beg, Iter end) {
	while (beg != end) {
		if (isSuffix(str, *beg))
			return true;
		++beg;
	}
	return false;
}

std::string htmlSpecialChars(const std::string& s);

bool isInteger(const std::string& s, size_t beg = 0,
	size_t end = std::string::npos);

bool isReal(const std::string& s, size_t beg = 0,
	size_t end = std::string::npos);

/* Converts s: [beg, end) to size_t
*  if end > s.size() or end == string::npos then end = s.size()
*  if beg > end then beg = end
*  if x == NULL then only validate
*  Return value: -1 if s: [beg, end) is not number
*    otherwise number of characters parsed
*/
template<class T>
int strtoi(const std::string& s, T *x, size_t beg = 0,
		size_t end = std::string::npos) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	if (x == NULL)
		return isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	return end - beg;
}

template<class T>
int strtou(const std::string& s, T *x, size_t beg = 0,
		size_t end = std::string::npos) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	if (x == NULL)
		return s[beg] != '-' && isInteger(s, beg, end) ? end - beg : -1;

	*x = 0;
	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			*x = *x * 10 + s[i] - '0';
		else
			return -1;
	}

	return end - beg;
}

inline long long strtoll(const std::string& s, size_t beg = 0,
		size_t end = std::string::npos) {
	long long x;
	strtoi(s, &x, beg, end);
	return x;
}

inline unsigned long long strtoull(const std::string& s, size_t beg = 0,
		size_t end = std::string::npos) {
	unsigned long long x;
	strtou(s, &x, beg, end);
	return x;
}

/**
 * @brief Converts usec (ULL) to sec (double as string)
 *
 * @param x usec value
 * @param prec precision (maximum number of digits after '.')
 * @param trim_nulls set whether trim trailing nulls
 * @return floating-point x in sec as string
 */
std::string usecToSecStr(unsigned long long x, unsigned prec,
	bool trim_nulls = true);

/**
 * @brief String comparator
 * @details Compares strings like numbers
 */
struct StrNumCompare {
	bool operator()(const std::string& a, const std::string& b) const {
		return a.size() == b.size() ? a < b : a.size() < b.size();
	}
};
