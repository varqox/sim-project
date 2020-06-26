#pragma once

#include <utility>

template <class Func>
class CallInDtor {
	Func func_;
	bool make_call_ = true;

public:
	CallInDtor(Func func) try : func_(std::move(func)) {
	} catch (...) {
		func();
		throw;
	}

	CallInDtor(const CallInDtor&) = delete;
	CallInDtor& operator=(const CallInDtor&) = delete;

	CallInDtor(CallInDtor&& cid) : func_(std::move(cid.func_)) { cid.cancel(); }

	CallInDtor& operator=(CallInDtor&& cid) {
		func_ = std::move(cid.func_);
		cid.cancel();
		return *this;
	}

	bool active() const noexcept { return make_call_; }

	void cancel() noexcept { make_call_ = false; }

	void restore() noexcept { make_call_ = true; }

	auto call_and_cancel() {
		make_call_ = false;
		return func_();
	}

	~CallInDtor() {
		if (make_call_) {
			try {
				func_();
			} catch (...) {
			} // We cannot throw
		}
	}
};
