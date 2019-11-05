#pragma once

#include "inplace_buff.hh"

// SHA-3

// Returns 48 bytes long hash ([a-f0-9]+)
InplaceBuff<56> sha3_224(StringView str);

// Returns 64 bytes long hash ([a-f0-9]+)
InplaceBuff<64> sha3_256(StringView str);

// Returns 96 bytes long hash ([a-f0-9]+)
InplaceBuff<96> sha3_384(StringView str);

// Returns 128 bytes long hash ([a-f0-9]+)
InplaceBuff<128> sha3_512(StringView str);

extern "C" {

void Keccak(unsigned int rate, unsigned int capacity,
            const unsigned char* input, unsigned long long int inputByteLen,
            unsigned char delimitedSuffix, unsigned char* output,
            unsigned long long int outputByteLen);

} // extern C

/**
 *  Function to compute SHAKE128 on the input message with any output length.
 */
inline void FIPS202_SHAKE128(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output,
                             int outputByteLen) {
	Keccak(1344, 256, input, inputByteLen, 0x1F, output, outputByteLen);
}

/**
 *  Function to compute SHAKE256 on the input message with any output length.
 */
inline void FIPS202_SHAKE256(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output,
                             int outputByteLen) {
	Keccak(1088, 512, input, inputByteLen, 0x1F, output, outputByteLen);
}

/**
 *  Function to compute SHA3-224 on the input message. The output length is
 * fixed to 28 bytes.
 */
inline void FIPS202_SHA3_224(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output) {
	Keccak(1152, 448, input, inputByteLen, 0x06, output, 28);
}

/**
 *  Function to compute SHA3-256 on the input message. The output length is
 * fixed to 32 bytes.
 */
inline void FIPS202_SHA3_256(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output) {
	Keccak(1088, 512, input, inputByteLen, 0x06, output, 32);
}

/**
 *  Function to compute SHA3-384 on the input message. The output length is
 * fixed to 48 bytes.
 */
inline void FIPS202_SHA3_384(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output) {
	Keccak(832, 768, input, inputByteLen, 0x06, output, 48);
}

/**
 *  Function to compute SHA3-512 on the input message. The output length is
 * fixed to 64 bytes.
 */
inline void FIPS202_SHA3_512(const unsigned char* input,
                             unsigned int inputByteLen, unsigned char* output) {
	Keccak(576, 1024, input, inputByteLen, 0x06, output, 64);
}
