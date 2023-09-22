#pragma once

#include "../communication/supervisor_pid1_tracee.hh"
#include "../supervisor/request/request.hh"

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

    int proc_dirfd;
    int tracee_cgroup_cpu_stat_fd;
    std::optional<int> signal_pid1_to_setup_tracee_cpu_timer_fd;
    int fd_to_close_upon_execve;

    struct LinuxNamespaces {
        struct User {
            uid_t outside_uid;
            uid_t inside_uid;
            gid_t outside_gid;
            gid_t inside_gid;
        } user;
    } linux_namespaces;

    using Prlimit = supervisor::request::Request::Prlimit;
    Prlimit prlimit;
    std::optional<int> seccomp_bpf_fd;
};

[[noreturn]] void main(Args args) noexcept;

} // namespace sandbox::tracee
