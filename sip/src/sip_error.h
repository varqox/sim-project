#pragma once

#include <simlib/logger.h>
#include <simlib/string.h>

template<class... Args>
auto log_warning(Args&&... args) {
	return stdlog("\033[1;35mwarning\033[m: ", std::forward<Args>(args)...);
}

class SipError : protected std::runtime_error {
public:
	template<class... Args>
	SipError(Args&&... args)
		: std::runtime_error(concat_tostr(std::forward<Args>(args)...)) {}

	SipError(const SipError&) = default;
	SipError(SipError&&) = default;
	SipError& operator=(const SipError&) = default;
	SipError& operator=(SipError&&) = default;

	using std::runtime_error::what;

	virtual ~SipError() = default;
};
