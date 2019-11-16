#pragma once

#include <simlib/string_view.hh>

inline bool is_subsequence(StringView sequence, StringView str) noexcept {
	if (sequence.empty())
		return true;

	size_t i = 0;
	for (char c : str)
		if (c == sequence[i] and ++i == sequence.size())
			return true;

	return false;
}

inline bool matches_pattern(StringView pattern, StringView str) noexcept {
	// Match as a subsequence whole str if pattern contains '.'
	if (pattern.find('.') != StringView::npos)
		return is_subsequence(pattern, str);

	// Otherwise match as a subsequence the part of str before last '.'
	auto pos = str.rfind('.');
	if (pos == StringView::npos)
		pos = str.size();

	return is_subsequence(pattern, str.substring(0, pos));
}

inline bool not_isspace(int c) noexcept { return not isspace(c); }
