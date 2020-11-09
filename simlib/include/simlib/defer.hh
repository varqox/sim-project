#pragma once

#include <utility>

template <class Func>
class Defer {
	Func func_;

public:
	// NOLINTNEXTLINE(google-explicit-constructor)
	Defer(Func func) try
	: func_(std::move(func))
	{
	} catch (...) {
		func();
		throw;
	}

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
