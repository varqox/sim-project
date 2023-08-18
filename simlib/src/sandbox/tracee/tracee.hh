#pragma once

#include "../communication/supervisor_pid1_tracee.hh"

#include <optional>
#include <vector>

namespace sandbox::tracee {

struct Args {
    volatile communication::supervisor_pid1_tracee::SharedMemState* shared_mem_state;
    int executable_fd;
    std::optional<int> stdin_fd;
    std::optional<int> stdout_fd;
    std::optional<int> stderr_fd;
    std::vector<char*> argv; // with a trailing nullptr element
    std::vector<char*> env; // with a trailing nullptr element

    int tracee_cgroup_cpu_stat_fd;
};

[[noreturn]] void main(Args args) noexcept;

} // namespace sandbox::tracee
