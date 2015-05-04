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
