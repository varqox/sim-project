#pragma once

#include <simlib/logger.hh>

// E.g. use:
// static DebugLogger<true> debuglog;
// debuglog("sth: ", sth);
template <bool enable, bool be_verbose = false, bool use_colors = true>
struct DebugLogger {
    static constexpr bool is_enabled = enable;
    static constexpr bool is_verbose = be_verbose;
    static constexpr bool is_colored = use_colors;

    template <class... Args>
    void operator()(Args&&... args) const noexcept {
        if constexpr (is_enabled) {
            try {
                if constexpr (use_colors) {
                    stdlog("\033[33m", args..., "\033[m");
                } else {
                    stdlog(args...);
                }
            } catch (...) {
                // Ignore errors
            }
        }
    }

    template <class... Args>
    void verbose(Args&&... args) const noexcept {
        if constexpr (is_enabled and is_verbose) {
            try {
                if constexpr (use_colors) {
                    stdlog("\033[2m", args..., "\033[m");
                } else {
                    stdlog(args...);
                }
            } catch (...) {
                // Ignore errors
            }
        }
    }
};
