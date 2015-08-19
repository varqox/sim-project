#pragma once

#include "STD_SHA_256.h"

namespace sha {

inline std::string sha256(const std::string& str) {
	return STD_SHA_256::Digest(str);
}

} // namespace sha

using sha::sha256;
