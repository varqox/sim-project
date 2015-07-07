#include "../include/string.h"

#include <algorithm>

using std::string;

string toString(long long a) {
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
	else if (w.empty())
		w = "0";
	else
		std::reverse(w.begin(), w.end());

	return w;
}

string toString(unsigned long long a) {
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

string htmlSpecialChars(const string& s) {
	string res;

	for (size_t i = 0; i < s.size(); ++i)
		switch (s[i]) {
		case '&':
			res += "&amp;";
			break;

		case '"':
			res += "&quot;";
			break;

		case '\'':
			res += "&apos;";
			break;

		case '<':
			res += "&lt;";
			break;

		case '>':
			res += "&gt;";
			break;

		default:
			res += s[i];
		}

	return res;
}

bool isInteger(const std::string& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	if (beg < end && s[beg] == '-')
		++beg;

	for (; beg < end; ++beg)
		if (!isdigit(s[beg]))
			return false;

	return true;
}

bool isReal(const std::string& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	if (beg < end && s[beg] == '-')
		++beg;

	bool dot = false;
	for (; beg < end; ++beg)
		if (!isdigit(s[beg])) {
			if (s[beg] == '.' && !dot)
				dot = true;
			else
				return false;
		}

	return true;
}

string usecToSecStr(unsigned long long x, unsigned prec, bool trim_nulls) {
	unsigned long long y = x / 1000000;
	string res = toString(y);

	y = x - y * 1000000;
	char t[7] = "000000";
	for (int i = 5; i >= 0; --i) {
		t[i] = '0' + y % 10;
		y /= 10;
	}

	// Truncate trailing zeros
	if (trim_nulls)
		for (int i = std::min(5u, prec - 1); i >= 0 && t[i] == '0'; --i)
			t[i] = '\0';

	if (prec < 6)
		t[prec] = '\0';

	if (t[0] != '\0')
		res.append(".").append(t);

	return res;
};
