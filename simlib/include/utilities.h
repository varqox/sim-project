#pragma once

#include <algorithm>
#include <string>
#include <vector>

template<class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
	reference.reserve(reference.size() + sizeof...(args));
	int t[] = {(reference.emplace_back(std::forward<Args>(args)), 0)...};
	(void)t;
	return reference;
}

template<class... Args>
std::string& back_insert(std::string& str, Args&&... args) {
	size_t total_length = str.size();
	int foo[] = {(total_length += string_length(args), 0)...};
	(void)foo;

	str.reserve(total_length);
	int bar[] = {(str += std::forward<Args>(args), 0)...};
	(void)bar;
	return str;
}

#if __cplusplus > 201103L
# warning "Delete the class below (there is that feature in c++14)"
#endif
class less {
	template<class A, class B>
	bool operator()(A&& a, B&& b) const { return a < b; }
};

template<class T, class C>
typename T::const_iterator binaryFind(const T& x, const C& val) noexcept {
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

template<class T, class C, class Comp>
typename T::const_iterator binaryFind(const T& x, const C& val, Comp comp)
	noexcept
{
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp(*mid, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && *beg == val ? beg : x.end());
}

template<class T, typename B, class C>
typename T::const_iterator binaryFindBy(const T& x, B T::value_type::*field,
	const C& val) noexcept
{
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

template<class T, typename B, class C, class Comp>
typename T::const_iterator binaryFindBy(const T& x, B T::value_type::*field,
	const C& val, Comp comp) noexcept
{
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

template<class A, class B>
inline bool binary_search(const A& a, B&& val) {
	return std::binary_search(a.begin(), a.end(), std::forward<B>(val));
}

template<class A, class B>
inline auto lower_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::lower_bound(a.begin(), a.end(), std::forward<B>(val));
}

template<class A, class B>
inline auto upper_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::upper_bound(a.begin(), a.end(), std::forward<B>(val));
}

template<class Func>
class CallInDtor {
	Func func;

public:
	explicit CallInDtor(Func f) : func(f) {}

	CallInDtor(const CallInDtor&) = delete;
	CallInDtor(CallInDtor&&) = delete;
	CallInDtor& operator=(const CallInDtor&) = delete;
	CallInDtor& operator=(CallInDtor&&) = delete;

	~CallInDtor() { func(); }
};
