#include "../include/sha.h"
#include "../include/string.h"

using std::string;

string sha3_224(StringView str) {
	unsigned char out[28];
	FIPS202_SHA3_224((const unsigned char*)(str.data()), str.size(), out);
	return toHex((const char*)out, 28);
}

string sha3_256(StringView str) {
	unsigned char out[32];
	FIPS202_SHA3_256((const unsigned char*)(str.data()), str.size(), out);
	return toHex((const char*)out, 32);
}

string sha3_384(StringView str) {
	unsigned char out[48];
	FIPS202_SHA3_384((const unsigned char*)(str.data()), str.size(), out);
	return toHex((const char*)out, 96);
}

string sha3_512(StringView str) {
	unsigned char out[64];
	FIPS202_SHA3_512((const unsigned char*)(str.data()), str.size(), out);
	return toHex((const char*)out, 64);
}
