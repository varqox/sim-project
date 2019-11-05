#pragma once

#include <utility>

template <class Func>
class CallInDtor {
	Func func;
	bool make_call = true;

public:
	CallInDtor(Func&& f) try : func(f) {
	} catch (...) {
		f();
		throw;
	}

	CallInDtor(const Func& f) try : func(f) {
	} catch (...) {
		f();
		throw;
	}

	CallInDtor(const CallInDtor&) = delete;
	CallInDtor& operator=(const CallInDtor&) = delete;

	CallInDtor(CallInDtor&& cid) : func(std::move(cid.func)) { cid.cancel(); }

	CallInDtor& operator=(CallInDtor&& cid) {
		func = std::move(cid.func);
		cid.cancel();
		return *this;
	}

	bool active() const noexcept { return make_call; }

	void cancel() noexcept { make_call = false; }

	void restore() noexcept { make_call = true; }

	auto call_and_cancel() {
		make_call = false;
		return func();
	}

	~CallInDtor() {
		if (make_call) {
			try {
				func();
			} catch (...) {
			} // We cannot throw
		}
	}
};
