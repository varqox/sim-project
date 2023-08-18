#pragma once

#include "../communication/supervisor_pid1_tracee.hh"

#include <optional>
#include <vector>

namespace sandbox::pid1 {

struct Args {
    volatile communication::supervisor_pid1_tracee::SharedMemState* shared_mem_state;
    int executable_fd;
    std::optional<int> stdin_fd;
    std::optional<int> stdout_fd;
    std::optional<int> stderr_fd;
    std::vector<char*> argv; // with a trailing nullptr element
    std::vector<char*> env; // with a trailing nullptr element
    int supervisor_pidfd;
    int tracee_cgroup_fd;

    struct LinuxNamespaces {
        struct User {
            uid_t outside_uid;
            uid_t inside_uid;
            gid_t outside_gid;
            gid_t inside_gid;
        } user;
    } linux_namespaces;
};

[[noreturn]] void main(Args args) noexcept;

} // namespace sandbox::pid1
