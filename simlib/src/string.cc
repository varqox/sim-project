#include "../include/string.h"

using std::string;

bool slowEqual(const char* str1, const char* str2, size_t len) noexcept {
	char rc = 0;
	for (size_t i = 0; i < len; ++i)
		rc |= str1[i] ^ str2[i];

	return rc == 0;
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
			ret += dectoHex(c >> 4);
			ret += dectoHex(c & 15);
		}
	}

	return ret;
}

string decodeURI(const StringView& str, size_t beg, size_t end) {
	if (end > str.size())
		end = str.size();

	string ret;
	if (end > 2)
		for (size_t end2 = end - 2; beg < end2; ++beg) {

			if (str[beg] == '%') {
				ret += static_cast<char>((hextodec(str[beg + 1]) << 4) +
					hextodec(str[beg + 2]));
				beg += 2;
			} else if (str[beg] == '+')
				ret += ' ';
			else
				ret += str[beg];
		}

	// last two characters (if left)
	for (; beg < end; ++beg) {
		if (str[beg] == '+')
			ret += ' ';
		else
			ret += str[beg];
	}

	return ret;
}

string toHex(const char* str, size_t len) {
	string res(len << 1, '\0');
	for (size_t i = -1; len-- > 0; ++str) {
		unsigned char c = *str;
		res[++i] = dectohex(c >> 4);
		res[++i] = dectohex(c & 15);
	}

	return res;
}

void appendHtmlEscaped(string& str, const StringView& s) {
	for (size_t i = 0; i < s.size(); ++i)
		appendHtmlEscaped(str, s[i]);
}

string usecToSecStr(uint64_t x, uint prec, bool trim_zeros) {
	uint64_t y = x / 1000000;
	string res = toStr(y);

	y = x - y * 1000000;
	char t[7] = "000000";
	for (int i = 5; i >= 0; --i) {
		t[i] = '0' + y % 10;
		y /= 10;
	}

	// Truncate trailing zeros
	if (trim_zeros)
		for (int i = std::min(5u, prec - 1); i >= 0 && t[i] == '0'; --i)
			t[i] = '\0';

	if (prec < 6)
		t[prec] = '\0';

	if (t[0] != '\0')
		res.append(1, '.').append(t);

	return res;
}

string widenedString(const StringView& s, size_t len, Adjustment adj,
	char fill)
{
	string res;
	if (adj == LEFT && len > s.size()) {
		res.assign(s.data(), s.size());
		res.resize(res.size() + len - s.size(), fill);

	} else {
		if (len > s.size())
			res.resize(len - s.size(), fill);
		res.append(s.data(), s.size());
	}

	return res;
}
