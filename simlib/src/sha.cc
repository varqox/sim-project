#include "../include/sha.h"
#include "../include/string.h"

using std::string;

string sha3_224(const string& str) {
	unsigned char out[28], i = -1;
	FIPS202_SHA3_224((const unsigned char*)(str.data()), str.size(), out);

	string res(48, '\0');
	for (auto c : out) {
		res[++i] = dectohex2(c >> 4);
		res[++i] = dectohex2(c & 15);
	}

	return res;
}

string sha3_256(const string& str) {
	unsigned char out[32], i = -1;
	FIPS202_SHA3_256((const unsigned char*)(str.data()), str.size(), out);

	string res(64, '\0');
	for (auto c : out) {
		res[++i] = dectohex2(c >> 4);
		res[++i] = dectohex2(c & 15);
	}

	return res;
}

string sha3_384(const string& str) {
	unsigned char out[48], i = -1;
	FIPS202_SHA3_384((const unsigned char*)(str.data()), str.size(), out);

	string res(96, '\0');
	for (auto c : out) {
		res[++i] = dectohex2(c >> 4);
		res[++i] = dectohex2(c & 15);
	}

	return res;
}

string sha3_512(const string& str) {
	unsigned char out[64], i = -1;
	FIPS202_SHA3_512((const unsigned char*)(str.data()), str.size(), out);

	string res(128, '\0');
	for (auto c : out) {
		res[++i] = dectohex2(c >> 4);
		res[++i] = dectohex2(c & 15);
	}

	return res;
}
