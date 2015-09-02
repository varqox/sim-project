#include "../include/string.h"

#include <algorithm>

using std::string;

StringView::size_type StringView::find(const StringView& s) const {
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

StringView::size_type StringView::find(char c, size_t beg, size_t endi) const {
	if (endi > len)
		endi = len;

	for (; beg < endi; ++beg)
		if (str[beg] == c)
			return beg;

	return npos;
}

StringView::size_type StringView::rfind(const StringView& s) const {
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

StringView::size_type StringView::rfind(char c, size_t beg, size_t endi) const {
	if (endi > len)
		endi = len;

	for (--endi; endi >= beg; --endi)
		if (str[endi] == c)
			return endi;

	return npos;
}

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

int strtonum(string& x, const StringView& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	for (size_t i = beg; i < end; ++i)
		if (!isdigit(s[i]))
			return -1;

	s.substr(beg, end - beg).to_string().swap(x);
	return end - beg;
}

int strToNum(string& x, const StringView& s, size_t beg, char c) {
	if (beg > s.size()) {
		x.clear();
		return 0;
	}

	size_t end = beg;
	for (size_t i = beg, len = s.size(); i < len && s[i] != c; ++i, ++end)
		if (!isdigit(s[i]))
			return -1;

	s.substr(beg, end - beg).to_string().swap(x);
	return end - beg;
}

string encodeURI(const StringView& str, size_t beg, size_t end) {
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

string decodeURI(const StringView& str, size_t beg, size_t end) {
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

string htmlSpecialChars(const StringView& s) {
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

bool isInteger(const StringView& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return false; // empty string is not integer

	if ((s[beg] == '-' || s[beg] == '+') && ++beg == end)
			return false; // sign is not integer

	for (; beg < end; ++beg)
		if (!isdigit(s[beg]))
			return false;

	return true;
}

bool isDigit(const StringView& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg >= end)
		return false;

	for (; beg < end; ++beg)
		if (!isdigit(s[beg]))
			return false;

	return true;
}

bool isReal(const StringView& s, size_t beg, size_t end) {
	if (end > s.size())
		end = s.size();
	if (beg > end)
		beg = end;

	if (beg < end && (s[beg] == '-' || s[beg] == '+'))
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
