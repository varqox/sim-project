#pragma once

#include <algorithm>
#include <simlib/string_view.hh>

inline bool isUsername(StringView str) noexcept {
	return std::all_of(str.begin(), str.end(), [](int x) {
		return (isalnum(x) || x == '_' || x == '-');
	});
}
