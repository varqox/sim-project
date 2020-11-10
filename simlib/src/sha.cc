#include "simlib/sha.hh"
#include "simlib/string_transform.hh"

extern "C" {
#include <3rdparty/sha3.c>
}

InplaceBuff<56> sha3_224(StringView str) {
    unsigned char out[28];
    FIPS202_SHA3_224(reinterpret_cast<const unsigned char*>(str.data()), str.size(), out);
    return to_hex<56>({reinterpret_cast<const char*>(out), 28});
}

InplaceBuff<64> sha3_256(StringView str) {
    unsigned char out[32];
    FIPS202_SHA3_256(reinterpret_cast<const unsigned char*>(str.data()), str.size(), out);
    return to_hex<64>({reinterpret_cast<const char*>(out), 32});
}

InplaceBuff<96> sha3_384(StringView str) {
    unsigned char out[48];
    FIPS202_SHA3_384(reinterpret_cast<const unsigned char*>(str.data()), str.size(), out);
    return to_hex<96>({reinterpret_cast<const char*>(out), 48});
}

InplaceBuff<128> sha3_512(StringView str) {
    unsigned char out[64];
    FIPS202_SHA3_512(reinterpret_cast<const unsigned char*>(str.data()), str.size(), out);
    return to_hex<128>({reinterpret_cast<const char*>(out), 64});
}
