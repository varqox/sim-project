#pragma once

#include "../communication/supervisor_pid1_tracee.hh"
#include "../supervisor/request/request.hh"

#include <cstdint>
#include <ctime>
#include <linux/filter.h>
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
    int tracee_cgroup_kill_fd;
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

    std::optional<timespec> time_limit;
    std::optional<timespec> cpu_time_limit;
    double max_tracee_parallelism; // in threads (1.5 = 1.5 parallel threads)
    bool tracee_is_restricted_to_single_thread;

    sock_fprog seccomp_filter;
};

[[noreturn]] void main(Args args) noexcept;

} // namespace sandbox::pid1
