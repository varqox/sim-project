#include "../include/string.h"

#include <algorithm>

using std::string;

string myto_string(long long a) {
	string w;
	bool minus = false;
	if (a < 0) {
		minus = true;
		a = -a;
	}
	while (a > 0) {
		w += static_cast<char>(a % 10 + '0');
		a /= 10;
	}
	if (minus)
		w += "-";
	if (w.empty())
		w = "0";
	else
		std::reverse(w.begin(), w.end());
return w;
}

string myto_string(size_t a) {
	string w;
	while (a > 0) {
		w += static_cast<char>(a % 10 + '0');
		a /= 10;
	}
	if (w.empty())
		w = "0";
	else
		std::reverse(w.begin(), w.end());
return w;
}

size_t strtosize_t(const string& s) {
	size_t res = 0;
	for (size_t i = 0; i < s.size(); ++i)
		res = res * 10 + s[i] - '0';
	return res;
}

int strtosize_t(size_t& x, const string& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	x = 0;
	for (size_t i = beg; i < end; ++i) {
		if (isdigit(s[i]))
			x = x * 10 + s[i] - '0';
		else
			return -1;
	}
	return end - beg;
}

int strtonum(string& x, const string& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	for (size_t i = beg; i < end; ++i)
		if (!isdigit(s[i]))
			return -1;
	s.substr(beg, end - beg).swap(x);
	return end - beg;
}

size_t find(const string& str, char c, size_t beg, size_t end) {
	if (end > str.size())
		end = str.size();
	for (; beg < end; ++beg)
		if (str[beg] == c)
			return beg;
	return string::npos;
}

string encodeURI(const string& str, size_t beg, size_t end) {
	// a-z A-Z 0-9 - _ . ~
	static bool is_safe[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};
	if (end > str.size())
		end = str.size();
	string ret;
	for (; beg < end; ++beg) {
		unsigned char c = str[beg];
		if (is_safe[c])
			ret += c;
		else {
			ret += '%';
			ret += dectohex(c >> 4);
			ret += dectohex(c & 15);
		}
	}
	return ret;
}

string decodeURI(const string& str, size_t beg, size_t end) {
	if (end > str.size())
		end = str.size();
	string ret;
	for (; beg < end; ++beg) {
		if (str[beg] == '%' && beg + 2 < end) {
			ret += static_cast<char>((hextodec(str[beg+1]) << 4) + hextodec(str[beg+2]));
			beg += 2;
		} else if (str[beg] == '+')
			ret += ' ';
		else
			ret += str[beg];
	}
	return ret;
}

string tolower(string str) {
	for (size_t i = 0, s = str.size(); i < s; ++i)
		str[i] = tolower(str[i]);
	return str;
}

string abspath(const string& path, size_t beg, size_t end, string root) {
	if (end > path.size())
		end = path.size();
	while (beg < end) {
		while (beg < end && path[beg] == '/')
			++beg;
		size_t next_slash = std::min(end, find(path, '/', beg, end));
		// If [beg, next_slash) == ("." or "..")
		if ((next_slash - beg == 1 && path[beg] == '.') ||
				(next_slash - beg == 2 && path[beg] == '.' &&
				path[beg + 1] == '.')) {
			beg = next_slash;
			continue;
		}
		if (*--root.end() != '/')
			root += '/';
		root.append(path, beg, next_slash - beg);
		beg = next_slash;
	}
	return root;
}

string htmlSpecialChars(const string& s) {
	string res;
	for (size_t i = 0; i < s.size(); ++i)
		switch (s[i]) {
			case '&': res += "&amp;";
			case '"': res += "&quot;";
			case '\'': res += "&apos;";
			case '<': res += "&lt;";
			case '>': res += "&gt;";
			default: res += s[i];
		}
	return res;
}
