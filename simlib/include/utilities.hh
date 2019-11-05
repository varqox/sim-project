#pragma once

#include <algorithm>

template <class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
	// Allocate more space (reserve is inefficient)
	size_t old_size = reference.size();
	reference.resize(old_size + sizeof...(args));
	reference.resize(old_size);

	(reference.emplace_back(std::forward<Args>(args)), ...);
	return reference;
}

template <class T, class C>
constexpr typename T::const_iterator binary_find(const T& x, const C& val) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (*mid < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && *beg == val ? beg : x.end());
}

template <class T, class C, class Comp>
constexpr typename T::const_iterator binary_find(const T& x, const C& val,
                                                 Comp&& comp) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp(*mid, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && !comp(*beg, val) && !comp(val, *beg) ? beg
	                                                               : x.end());
}

template <class T, typename B, class C>
constexpr typename T::const_iterator
binary_find_by(const T& x, B T::value_type::*field, const C& val) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if ((*mid).*field < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

template <class T, typename B, class C, class Comp>
constexpr typename T::const_iterator
binary_find_by(const T& x, B T::value_type::*field, const C& val, Comp&& comp) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp((*mid).*field, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

// Aliases - take container instead of range
template <class A, class B>
inline bool binary_search(const A& a, B&& val) {
	return std::binary_search(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto lower_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::lower_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto upper_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::upper_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A>
inline void sort(A& a) {
	std::sort(a.begin(), a.end());
}

template <class A, class Func>
inline void sort(A& a, Func&& func) {
	std::sort(a.begin(), a.end(), std::forward<Func>(func));
}

template <class T>
constexpr bool is_sorted(T& collection) {
	auto b = begin(collection);
	auto e = end(collection);
	if (b == e)
		return true;

	auto prev = b;
	while (++b != e) {
		if (not std::less<>()(*prev, *b))
			return false;

		prev = b;
	}

	return true;
}

template <class A, class... Option>
constexpr bool is_one_of(A&& val, Option&&... option) {
	return ((val == option) or ...);
}

template <class...>
constexpr bool is_pair = false;

template <class A, class B>
constexpr bool is_pair<std::pair<A, B>> = true;

template <class T>
class reverse_view {
	T range_;

public:
	reverse_view(T&& range) : range_(std::forward<T>(range)) {}

	auto begin() const { return range_.rbegin(); }

	auto end() const { return range_.rend(); }
};

template <class T>
reverse_view(T &&)->reverse_view<T>;
