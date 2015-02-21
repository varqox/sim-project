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

string encodeURI(const string& str, size_t beg, size_t end) {
	// a-z A-Z 0-9 - _ . ~
	static bool is_safe[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};
	if (end == string::npos)
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
	if (end == string::npos)
		end = str.size();
	string ret;
	for (; beg < end; ++beg) {
		if (str[beg] == '%' && beg + 2 < end)
			ret += static_cast<char>((hextodec(str[beg+1]) << 4) + hextodec(str[beg+2])), beg += 2;
		else if (str[beg] == '+')
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
