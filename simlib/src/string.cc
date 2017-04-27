#include "../include/string.h"

using std::string;

bool slowEqual(const char* str1, const char* str2, size_t len) noexcept {
	char rc = 0;
	for (size_t i = 0; i < len; ++i)
		rc |= str1[i] ^ str2[i];

	return rc == 0;
}

string encodeURI(StringView str, size_t beg, size_t end) {
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

string toHex(const char* str, size_t len) {
	string res(len << 1, '\0');
	for (size_t i = -1; len-- > 0; ++str) {
		unsigned char c = *str;
		res[++i] = dectohex(c >> 4);
		res[++i] = dectohex(c & 15);
	}

	return res;
}

void appendHtmlEscaped(string& str, StringView s) {
	for (size_t i = 0; i < s.size(); ++i)
		appendHtmlEscaped(str, s[i]);
}

string paddedString(StringView s, size_t len, Adjustment adj, char filler) {
	string res;
	if (adj == LEFT && len > s.size()) {
		res.assign(s.data(), s.size());
		res.resize(res.size() + len - s.size(), filler);

	} else {
		if (len > s.size())
			res.resize(len - s.size(), filler);
		res.append(s.data(), s.size());
	}

	return res;
}
