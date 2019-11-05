#pragma once

#include <utility>

template <class Func>
class Defer {
	Func func_;

public:
	Defer(Func func) : func_(std::move(func)) {}

	Defer(const Defer&) = delete;
	Defer(Defer&&) = delete;
	Defer& operator=(const Defer&) = delete;
	Defer& operator=(Defer&&) = delete;

	~Defer() {
		try {
			func_();
		} catch (...) {
		}
	}
};
