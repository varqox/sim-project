#pragma once

#include <simlib/string.h>

inline bool is_subsequence(StringView sequence, StringView str) noexcept {
	if (sequence.empty())
		return true;

	size_t i = 0;
	for (char c : str)
		if (c == sequence[i] and ++i == sequence.size())
			return true;

	return false;
}

inline bool not_isspace(int c) noexcept { return not isspace(c); }
