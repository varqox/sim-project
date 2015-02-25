#pragma once

#include <string>
#include <cctype>

std::string myto_string(long long a);
std::string myto_string(size_t a);
size_t strtosize_t(const std::string& s);

// Like string::find but searches in [beg, end)
size_t find(const std::string& str, char c, size_t beg = 0, size_t end = std::string::npos);

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

// Return an absolute path that does not contain any . or .. components, nor any repeated path separators (/)
std::string abspath(const std::string& path, size_t beg = 0, size_t end = std::string::npos, std::string root = "/");
