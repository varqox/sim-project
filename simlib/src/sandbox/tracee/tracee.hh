#pragma once

#include "../communication/supervisor_tracee.hh"

#include <optional>
#include <vector>

namespace sandbox::tracee {

struct Args {
    volatile communication::supervisor_tracee::SharedMemState* supervisor_shared_mem_state;
    int executable_fd;
    std::optional<int> stdin_fd;
    std::optional<int> stdout_fd;
    std::optional<int> stderr_fd;
    std::vector<char*> argv; // with a trailing nullptr element
    std::vector<char*> env; // with a trailing nullptr element

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

} // namespace sandbox::tracee
