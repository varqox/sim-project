#pragma once

#include "simlib/logger.hh"

template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
auto log_warning(Args&&... args) {
    return stdlog("\033[1;35mwarning\033[m: ", std::forward<Args>(args)...);
}

class SipError : protected std::runtime_error {
public:
    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    explicit SipError(Args&&... args)
    : std::runtime_error(concat_tostr(std::forward<Args>(args)...)) {}

    SipError(const SipError&) = default;
    SipError(SipError&&) = default;
    SipError& operator=(const SipError&) = default;
    SipError& operator=(SipError&&) = default;

    using std::runtime_error::what;

    ~SipError() override = default;
};
