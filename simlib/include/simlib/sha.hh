#pragma once

#include "simlib/inplace_buff.hh"

// SHA-3

// Returns 48 bytes long hash ([a-f0-9]+)
InplaceBuff<56> sha3_224(StringView str);

// Returns 64 bytes long hash ([a-f0-9]+)
InplaceBuff<64> sha3_256(StringView str);

// Returns 96 bytes long hash ([a-f0-9]+)
InplaceBuff<96> sha3_384(StringView str);

// Returns 128 bytes long hash ([a-f0-9]+)
InplaceBuff<128> sha3_512(StringView str);
