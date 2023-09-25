#include "../communication/supervisor_pid1_tracee.hh"
#include "../supervisor/cgroups/read_cpu_times.hh"
#include "tracee.hh"

#include <ctime>
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sched.h>
#include <simlib/converts_safely_to.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace sandbox::tracee {

[[noreturn]] void main(Args args) noexcept {
    namespace sms = communication::supervisor_pid1_tracee;
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        sms::write_result_error(
            args.shared_mem_state, "tracee: ", std::forward<decltype(msg)>(msg)...
        );
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
    };
    auto exclude_pid1_from_tracee_session_and_process_group = [&]() noexcept {
        if (setsid() < 0) {
            die_with_error("setsid()");
        }
    };
    auto write_file_at = [&](int dirfd, FilePath file_path, StringView data) noexcept {
        auto fd = openat(dirfd, file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (fd == -1) {
            die_with_error("openat(", file_path, ")");
        }
        if (write_all(fd, data) != data.size()) {
            die_with_error("write(", file_path, ")");
        }
        if (close(fd)) {
            die_with_error("close()");
        }
    };
    auto setup_user_namespace = [&](const Args::LinuxNamespaces::User& user_ns,
                                    int proc_dirfd) noexcept {
        write_file_at(
            proc_dirfd,
            "self/uid_map",
            from_unsafe{noexcept_concat(user_ns.inside_uid, ' ', user_ns.outside_uid, " 1")}
        );
        write_file_at(proc_dirfd, "self/setgroups", "deny");
        write_file_at(
            proc_dirfd,
            "self/gid_map",
            from_unsafe{noexcept_concat(user_ns.inside_gid, ' ', user_ns.outside_gid, " 1")}
        );
    };
    auto setup_std_fds = [&]() noexcept {
        if (args.stdin_fd && dup3(*args.stdin_fd, STDIN_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
        if (args.stdout_fd && dup3(*args.stdout_fd, STDOUT_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
        if (args.stderr_fd && dup3(*args.stderr_fd, STDERR_FILENO, 0) < 0) {
            die_with_error("dup3()");
        }
    };
    auto setup_prlimit = [&](const Args::Prlimit& pr) noexcept {
        auto setup_limit = [&](auto resource, auto opt) noexcept {
            if (opt) {
                rlimit64 rlim = {.rlim_cur = *opt, .rlim_max = *opt};
                if (prlimit64(0, resource, &rlim, nullptr)) {
                    die_with_error("prlimit()");
                }
            }
        };
        setup_limit(RLIMIT_AS, pr.max_address_space_size_in_bytes);
        setup_limit(RLIMIT_CORE, pr.max_core_file_size_in_bytes);
        setup_limit(RLIMIT_CPU, pr.cpu_time_limit_in_seconds);
        setup_limit(RLIMIT_FSIZE, pr.max_file_size_in_bytes);
        setup_limit(RLIMIT_NOFILE, pr.file_descriptors_num_limit);
        setup_limit(RLIMIT_STACK, pr.max_stack_size_in_bytes);
    };
    auto install_seccomp_filter = [&](std::optional<int> seccomp_bpf_fd) noexcept {
        if (not seccomp_bpf_fd) {
            return;
        }

        int fd = *seccomp_bpf_fd;
        auto fd_len = lseek64(fd, 0, SEEK_END);
        if (fd_len < 0) {
            die_with_error("lseek64()");
        }
        auto seccomp_filter_ptr = mmap(nullptr, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        if (seccomp_filter_ptr == MAP_FAILED) {
            die_with_error("mmap()");
        }
        if (fd_len % sizeof(sock_filter) != 0) {
            die_with_msg(
                "invalid seccomp_bpf_fd length: ",
                fd_len,
                " is not a multiple of ",
                sizeof(sock_filter)
            );
        }
        auto fprog_len = fd_len / sizeof(sock_filter);
        if (!converts_safely_to<decltype(sock_fprog::len)>(fprog_len)) {
            die_with_msg("seccomp_bpf_fd is too big");
        }
        auto fprog = sock_fprog{
            .len = static_cast<decltype(sock_fprog::len)>(fprog_len),
            .filter = reinterpret_cast<sock_filter*>(seccomp_filter_ptr),
        };
        if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &fprog)) {
            die_with_error("seccomp()");
        }
    };
    auto get_current_time = [&]() noexcept {
        timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
            die_with_error("clock_gettime()");
        }
        return ts;
    };
    auto get_current_cpu_times = [&]() noexcept {
        sched_yield(); // Needed for cgroup/cpu.stat to be updated and report more accurate values
        return supervisor::cgroups::read_cpu_times(
            args.tracee_cgroup_cpu_stat_fd,
            [&] [[noreturn]] (auto&&... msg) {
                die_with_msg("read_cpu_times(): ", std::forward<decltype(msg)>(msg)...);
            }
        );
    };
    auto save_exec_start_times = [&]() noexcept {
        // Real time needs to go before cpu time to ensure invariant for tracee's real time >= cpu
        // time
        auto ts = get_current_time();
        auto cpu_times = get_current_cpu_times();
        sms::write(args.shared_mem_state->tracee_exec_start_time, ts);
        sms::write(args.shared_mem_state->tracee_exec_start_cpu_time_user, cpu_times.user_usec);
        sms::write(args.shared_mem_state->tracee_exec_start_cpu_time_system, cpu_times.system_usec);
    };
    auto signal_pid1_that_exec_start_times_are_saved = [&]() noexcept {
        if (kill(1, SIGUSR2)) {
            die_with_error("kill()");
        }
        if (args.signal_pid1_to_setup_tracee_cpu_timer_fd &&
            write(*args.signal_pid1_to_setup_tracee_cpu_timer_fd, "", 1) < 0)
        {
            die_with_error("write()");
        }
    };

    exclude_pid1_from_tracee_session_and_process_group();
    setup_user_namespace(args.linux_namespaces.user, args.proc_dirfd);
    setup_std_fds();
    setup_prlimit(args.prlimit);
    install_seccomp_filter(args.seccomp_bpf_fd);

    if (args.argv.empty() || args.argv.back() != nullptr) {
        die_with_msg("BUG: argv array does not contain nullptr as the last element");
    }
    if (args.env.empty() || args.env.back() != nullptr) {
        die_with_msg("BUG: env array does not contain nullptr as the last element");
    }

    save_exec_start_times();
    signal_pid1_that_exec_start_times_are_saved();
    syscalls::execveat(args.executable_fd, "", args.argv.data(), args.env.data(), AT_EMPTY_PATH);
    die_with_error("execveat()");
}

} // namespace sandbox::tracee
