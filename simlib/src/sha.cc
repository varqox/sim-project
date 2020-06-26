#include "simlib/sha.hh"
#include "simlib/string_transform.hh"

InplaceBuff<56> sha3_224(StringView str) {
	unsigned char out[28];
	FIPS202_SHA3_224((const unsigned char*)(str.data()), str.size(), out);
	return to_hex<56>({(const char*)out, 28});
}

InplaceBuff<64> sha3_256(StringView str) {
	unsigned char out[32];
	FIPS202_SHA3_256((const unsigned char*)(str.data()), str.size(), out);
	return to_hex<64>({(const char*)out, 32});
}

InplaceBuff<96> sha3_384(StringView str) {
	unsigned char out[48];
	FIPS202_SHA3_384((const unsigned char*)(str.data()), str.size(), out);
	return to_hex<96>({(const char*)out, 48});
}

InplaceBuff<128> sha3_512(StringView str) {
	unsigned char out[64];
	FIPS202_SHA3_512((const unsigned char*)(str.data()), str.size(), out);
	return to_hex<128>({(const char*)out, 64});
}
