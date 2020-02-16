#pragma once

#include <simlib/string_view.hh>
#include <type_traits>

template<class T, class U>
inline bool is_subsequence(T&& subseqence, U&& sequence) noexcept {
	if (subseqence.empty())
		return true;

	size_t i = 0;
	for (auto const& x : sequence)
		if (x == subseqence[i] and ++i == subseqence.size())
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
