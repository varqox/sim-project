#pragma once

#include <algorithm>

inline bool isUsername(const std::string& str) noexcept {
	return all_of(str.begin(), str.end(), [](int x) {
		return (isalnum(x) || x == '_' || x == '-');
	});
}
