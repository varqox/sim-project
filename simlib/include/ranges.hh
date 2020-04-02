#pragma once

#include <iterator>

template <class T>
struct reverse_view_impl {
	int prevent_copy_elision_
	   [[maybe_unused]]; // Used to allow reverse_view(reverse_view(...))
	T range_;

	constexpr auto begin() { return range_.rbegin(); }
	constexpr auto end() { return range_.rend(); }
	constexpr auto rbegin() { return range_.begin(); }
	constexpr auto rend() { return range_.end(); }
};

template <class T>
reverse_view_impl(int, T &&)->reverse_view_impl<T&&>;

// This way it will work without with temporaries calling any constructor: e.g.
#define reverse_view(...)                                                      \
	reverse_view_impl { 0, __VA_ARGS__ }
