#pragma once

#include <cstdarg>
#include <vector>

template<class T>
std::vector<T> construct_vector(unsigned n, ...) {
	std::vector<T> res(n);
	va_list vl;
	va_start(vl,n);

	for (unsigned i = 0; i < n; ++i)
		res[i] = va_arg(vl, T);

	va_end(vl);
	return res;
}

template<class T>
class Appender {
public:
	T& ref;

	Appender(T& reference) : ref(reference) {}
	~Appender() {}

	template<class A>
	Appender& operator()(const A& x) {
		ref.insert(ref.end(), x);
		return *this;
	}

	template<class A>
	Appender& operator << (const A& x) {
		ref.insert(ref.end(), x);
		return *this;
	}
};

template<>
class Appender<std::string> {
public:
	std::string& ref;

	Appender(std::string& reference) : ref(reference) {}
	~Appender() {}

	template<class A>
	Appender& operator()(const A& x) {
		ref += x;
		return *this;
	}

	template<class A>
	Appender& operator <<(const A& x) {
		ref += x;
		return *this;
	}
};

template<class T>
Appender<T> append(T& reference) { return Appender<T>(reference); }

class less {
	template<class A, class B>
	bool operator()(const A& a, const B& b) { return a < b; }
};

template<class T, class C>
typename T::const_iterator binaryFind(const T& x, const C& val) {
	typename T::const_iterator beg = x.begin(), end = x.end(), mid;
	while (beg != end) {
		mid = beg + ((end - beg) >> 1);
		if (*mid < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && *beg == val ? beg : x.end());
}

template<class T, class C, class Comp>
typename T::const_iterator binaryFind(const T& x, const C& val, Comp comp) {
	typename T::const_iterator beg = x.begin(), end = x.end(), mid;
	while (beg != end) {
		mid = beg + ((end - beg) >> 1);
		if (comp(*mid, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && *beg == val ? beg : x.end());
}

template<class T, typename B, class C>
typename T::const_iterator binaryFindBy(const T& x, B T::value_type::*field,
		const C& val) {
	typename T::const_iterator beg = x.begin(), end = x.end(), mid;
	while (beg != end) {
		mid = beg + ((end - beg) >> 1);
		if ((*mid).*field < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

template<class T, typename B, class C, class Comp>
typename T::const_iterator binaryFindBy(const T& x, B T::value_type::*field,
		const C& val, Comp comp) {
	typename T::const_iterator beg = x.begin(), end = x.end(), mid;
	while (beg != end) {
		mid = beg + ((end - beg) >> 1);
		if (comp((*mid).*field, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}
