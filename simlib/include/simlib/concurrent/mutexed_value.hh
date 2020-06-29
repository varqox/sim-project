#pragma once

#include <mutex>
#include <type_traits>
#include <utility>

namespace concurrent {

template <class T>
class MutexedValue {
	std::mutex mtx_;
	T value_;

public:
	using value_type = T;

	MutexedValue(const MutexedValue&) = delete;
	MutexedValue(MutexedValue&&) = delete;
	MutexedValue& operator=(const MutexedValue&) = delete;
	MutexedValue& operator=(MutexedValue&&) = delete;

	~MutexedValue() = default;

	template <class... Args>
	explicit MutexedValue(Args&&... args)
	: value_(std::forward<Args>(args)...) {}

	std::pair<std::lock_guard<std::mutex>, T&> get() {
		return {std::lock_guard(mtx_), value_};
	}

	template <class Func>
	auto perform(Func&& operation) {
		static_assert(std::is_invocable_v<Func&&, T&>);
		std::lock_guard guard(mtx_);
		return std::forward<Func>(operation)(value_);
	}
};

} // namespace concurrent
