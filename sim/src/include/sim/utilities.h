#pragma once

#include <algorithm>
#include <simlib/string.h>

inline bool isUsername(StringView str) noexcept {
	return std::all_of(str.begin(), str.end(), [](int x) {
		return (isalnum(x) || x == '_' || x == '-');
	});
}

constexpr inline bool is_safe_timestamp(StringView str) noexcept {
	return isDigitNotGreaterThan<std::numeric_limits<time_t>::max()>(str);
}
