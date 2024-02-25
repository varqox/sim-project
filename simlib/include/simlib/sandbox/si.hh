#pragma once

#include <string>

namespace sandbox {

/// Describes exit status of the sandboxed process
struct Si {
    int code; // siginfo_t::si_code from waitid() of the root process
    int status; // siginfo_t::si_status from waitid() of the root process

    [[nodiscard]] std::string description() const;

    [[nodiscard]] bool operator==(const Si& other) const noexcept {
        return code == other.code && status == other.status;
    }

    [[nodiscard]] bool operator!=(const Si& other) const noexcept { return !(*this == other); }
};

} // namespace sandbox
