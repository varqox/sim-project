#pragma once

#include "to_string.hh"

template <class T>
constexpr auto string_length(const T& str) noexcept {
	return str.size();
}

constexpr auto string_length(const char* str) noexcept {
	return std::char_traits<char>::length(str);
}

constexpr auto string_length(char* str) noexcept {
	return std::char_traits<char>::length(str);
}

template <class T>
constexpr auto data(const T& x) noexcept {
	return x.data();
}

constexpr auto data(const char* x) noexcept { return x; }

constexpr auto data(char* x) noexcept { return x; }

template <class T>
constexpr decltype(auto) stringify(T&& x) {
	if constexpr (std::is_integral_v<
	                 std::remove_cv_t<std::remove_reference_t<T>>>) {
		return ::to_string(x);
	} else {
		return std::forward<T>(x);
	}
}
