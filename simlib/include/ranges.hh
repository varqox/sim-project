#pragma once

#include <iterator>

template <class T>
struct reverse_view_impl {
	int prevent_copy_elision_
	   [[maybe_unused]]; // Used to allow reverse_view(reverse_view(...))
	T range_;

	constexpr auto begin() { return std::rbegin(range_); }
	constexpr auto end() { return std::rend(range_); }
	constexpr auto rbegin() { return std::begin(range_); }
	constexpr auto rend() { return std::end(range_); }
};

template <class T>
reverse_view_impl(int, T &&)->reverse_view_impl<T&&>;

// This way it will work without with temporaries calling any constructor: e.g.
#define reverse_view(...)                                                      \
	reverse_view_impl { 0, __VA_ARGS__ }
