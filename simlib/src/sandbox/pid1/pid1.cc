#include "../communication/supervisor_pid1_tracee.hh"
#include "../tracee/tracee.hh"
#include "pid1.hh"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/ubsan.hh>
#include <sys/prctl.h>
#include <unistd.h>

namespace sandbox::pid1 {

[[noreturn]] void main(Args args) noexcept {
    namespace sms = communication::supervisor_pid1_tracee;
    auto die_with_msg = [&] [[noreturn]] (auto&&... msg) noexcept {
        sms::write_result_error(
            args.shared_mem_state, "pid1: ", std::forward<decltype(msg)>(msg)...
        );
        _exit(1);
    };
    auto die_with_error = [&] [[noreturn]] (auto&&... msg) noexcept {
        die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
    };
    auto set_process_name = [&]() noexcept {
        if (prctl(PR_SET_NAME, "pid1", 0, 0, 0)) {
            die_with_error("prctl(SET_NAME)");
        }
    };
    auto setup_kill_on_supervisor_death = [&](int supervisor_pidfd) noexcept {
        // Make kernel send us SIGKILL when the parent process (= supervisor process) dies
        if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0)) {
            die_with_error("prctl(PR_SET_PDEATHSIG)");
        }
        // Check if the supervisor is not already dead - it might happened just before prctl(). We
        // cannot use getppid() because it returns 0 as we are in a new PID namespace, so we use
        // poll() on supervisor's pidfd
        pollfd pfd = {
            .fd = supervisor_pidfd,
            .events = POLLIN,
            .revents = 0,
        };
        if (poll(&pfd, 1, 0) == 1) {
            die_with_msg("supervisor died");
        }
        // Close supervisor_pidfd to limit attack surface as it may be used to send signals to the
        // supervisor
        if (close(supervisor_pidfd)) {
            die_with_error("close()");
        }
    };
    auto setup_session_and_process_group = [&]() noexcept {
        // Exclude pid1 process from the parent's process group and session
        if (setsid() < 0) {
            die_with_error("setsid()");
        }
    };
    auto write_file = [&](FilePath file_path, StringView data) noexcept {
        auto fd = open(file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (fd == -1) {
            die_with_error("open(", file_path, ")");
        }
        if (write_all(fd, data) != data.size()) {
            die_with_error("write(", file_path, ")");
        }
        if (close(fd)) {
            die_with_error("close()");
        }
    };
    auto setup_user_namespace = [&](const Args::LinuxNamespaces::User& user_ns) noexcept {
        write_file(
            "/proc/self/uid_map",
            from_unsafe{noexcept_concat(user_ns.inside_uid, ' ', user_ns.outside_uid, " 1")}
        );
        write_file("/proc/self/setgroups", "deny");
        write_file(
            "/proc/self/gid_map",
            from_unsafe{noexcept_concat(user_ns.inside_gid, ' ', user_ns.outside_gid, " 1")}
        );
    };
    auto harden_against_potential_compromise = [&]() noexcept {
        // Cut access to cgroups other than ours
        unshare(CLONE_NEWCGROUP);

        // TODO: install seccomp filters
    };
    auto get_current_time = [&]() noexcept {
        timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
            die_with_error("clock_gettime()");
        }
        return ts;
    };

    set_process_name();
    setup_kill_on_supervisor_death(args.supervisor_pidfd);
    setup_session_and_process_group();
    setup_user_namespace(args.linux_namespaces.user);

    if (UNDEFINED_SANITIZER) {
        auto ignore_signal = [&](int sig) noexcept {
            struct sigaction sa = {};
            sa.sa_handler = SIG_IGN;
            if (sigemptyset(&sa.sa_mask)) {
                die_with_error("sigemptyset()");
            }
            sa.sa_flags = 0;
            if (sigaction(sig, &sa, nullptr)) {
                die_with_error("sigaction()");
            }
        };
        // Undefined sanitizer installs signal handlers for these signals and this leaves pid1
        // process prone to being killed by these signals
        ignore_signal(SIGBUS);
        ignore_signal(SIGFPE);
        ignore_signal(SIGSEGV);
    }

    clone_args cl_args = {};
    cl_args.flags = CLONE_NEWCGROUP | CLONE_INTO_CGROUP;
    cl_args.exit_signal = SIGCHLD;
    cl_args.cgroup = static_cast<uint64_t>(args.tracee_cgroup_fd);

    auto tracee_pid = syscalls::clone3(&cl_args);
    if (tracee_pid == -1) {
        die_with_error("clone3()");
    }
    if (tracee_pid == 0) {
        tracee::main({
            .shared_mem_state = args.shared_mem_state,
            .executable_fd = args.executable_fd,
            .stdin_fd = args.stdin_fd,
            .stdout_fd = args.stdout_fd,
            .stderr_fd = args.stderr_fd,
            .argv = std::move(args.argv),
            .env = std::move(args.env),
        });
    }

    // Close all file descriptors
    if (close_range(4, ~0U, 0)) {
        die_with_error("close_range()");
    }

    harden_against_potential_compromise();

    timespec waitid_time;
    siginfo_t si;
    for (;;) {
        if (syscalls::waitid(P_ALL, 0, &si, __WALL | WEXITED, nullptr)) {
            if (errno == ECHILD) {
                break;
            }
            die_with_error("waitid()");
        }
        waitid_time = get_current_time();
        if (si.si_pid == tracee_pid) {
            // Remaining processes will be killed on pid1's death
            break;
        }
    }

    // Check if tracee died prematurely with an error
    if (sms::is<sms::result::Error>(args.shared_mem_state)) {
        // Propagate error
        _exit(1); // error is already written by tracee
    }

    sms::write(args.shared_mem_state->tracee_waitid_time, waitid_time);
    sms::write_result_ok(
        args.shared_mem_state,
        {
            .code = si.si_code,
            .status = si.si_status,
        }
    );

    _exit(0);
}

} // namespace sandbox::pid1
