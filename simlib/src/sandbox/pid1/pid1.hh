#pragma once

#include "../communication/supervisor_pid1_tracee.hh"
#include "../supervisor/request/request.hh"

#include <cstdint>
#include <optional>
#include <sys/types.h>
#include <variant>
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
    int tracee_cgroup_cpu_stat_fd;

    struct LinuxNamespaces {
        struct User {
            struct Pid1 {
                uid_t outside_uid;
                uid_t inside_uid;
                gid_t outside_gid;
                gid_t inside_gid;
            } pid1;

            struct Tracee {
                uid_t outside_uid;
                uid_t inside_uid;
                gid_t outside_gid;
                gid_t inside_gid;
            } tracee;
        } user;

        using Mount = supervisor::request::Request::LinuxNamespaces::Mount;
        Mount mount;
    } linux_namespaces;

    using Prlimit = supervisor::request::Request::Prlimit;
    Prlimit prlimit;
};

[[noreturn]] void main(Args args) noexcept;

} // namespace sandbox::pid1
