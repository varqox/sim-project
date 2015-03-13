#pragma once

#include <string>
#include <cctype>

std::string myto_string(long long a);

std::string myto_string(size_t a);

size_t strtosize_t(const std::string& s);

/* Converts s: [beg, end) to size_t
*  if end > s.size() or end == string::npos then end = s.size()
*  if beg > end then beg = end
*  Return value: -1 if s: [beg, end) is not number
*    otherwise number of chracters parsed
*/
int strtosize_t(size_t& x, const std::string& s, size_t beg = 0, size_t end = std::string::npos);

// Like strtosize_t but places number into x
int strtonum(std::string& x, const std::string& s, size_t beg = 0, size_t end = std::string::npos);

// Like string::find but searches in [beg, end)
size_t find(const std::string& str, char c, size_t beg = 0, size_t end = std::string::npos);

inline int compare(const std::string& str, size_t beg, size_t end, const std::string& s) {
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;
	return str.compare(beg, end - beg, s);
}

inline int compare(const std::string& str, size_t beg, size_t end, const char* s) {
	if (end > str.size())
		end = str.size();
	if (beg > end)
		beg = end;
	return str.compare(beg, end - beg, s);
}

inline int compareTo(const std::string& str, size_t pos, char c, const std::string& s) {
	return compare(str, pos, str.find(c, pos) - pos, s);
}

inline int compareTo(const std::string& str, size_t pos, char c, const char* s) {
	return compare(str, pos, str.find(c, pos), s);
}

std::string encodeURI(const std::string& str, size_t beg = 0, size_t end = std::string::npos);

std::string decodeURI(const std::string& str, size_t beg = 0, size_t end = std::string::npos);

std::string tolower(std::string str);

inline int hextodec(char c) {
	c = tolower(c);
	return (c > 'a' ? 10 + c - 'a' : c - '0');
}

inline char dectohex(int x) { return x > 9 ? 'A' + x - 10 : x + '0'; }

inline bool isPrefix(const std::string& str, const std::string& prefix) {
	return str.compare(0, prefix.size(), prefix) == 0;
}

// Returns an absolute path that does not contain any . or .. components, nor any repeated path separators (/)
std::string abspath(const std::string& path, size_t beg = 0, size_t end = std::string::npos, std::string root = "/");

std::string htmlSpecialChars(const std::string& s);
