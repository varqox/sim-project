#pragma once

#include <memory>

template <class Func>
class shared_function {
	std::shared_ptr<Func> func_;

public:
	explicit shared_function(Func&& func)
	: func_(std::make_shared<Func>(std::forward<Func>(func))) {}

	template <class... Args>
	auto operator()(Args&&... args) const {
		return (*func_)(std::forward<Args>(args)...);
	}
};
