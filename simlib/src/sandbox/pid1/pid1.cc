#include "../communication/supervisor_pid1_tracee.hh"
#include "../supervisor/cgroups/read_cpu_times.hh"
#include "../tracee/tracee.hh"
#include "pid1.hh"
#include "simlib/file_perms.hh"

#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <optional>
#include <poll.h>
#include <sched.h>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_path.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/overloaded.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/timespec_arithmetic.hh>
#include <simlib/ubsan.hh>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <variant>

using std::optional;

namespace sms = sandbox::communication::supervisor_pid1_tracee;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sms::SharedMemState* shared_mem_state;

template <class... Args>
[[noreturn]] void die_with_msg(Args&&... msg) noexcept {
    sms::write_result_error(shared_mem_state, "pid1: ", std::forward<decltype(msg)>(msg)...);
    _exit(1);
}

template <class... Args>
[[noreturn]] void die_with_error(Args&&... msg) noexcept {
    die_with_msg(std::forward<decltype(msg)>(msg)..., errmsg());
}

void set_process_name() noexcept {
    if (prctl(PR_SET_NAME, "pid1", 0, 0, 0)) {
        die_with_error("prctl(SET_NAME)");
    }
}

void setup_kill_on_supervisor_death(int supervisor_pidfd) noexcept {
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
}

void setup_session_and_process_group() noexcept {
    // Exclude pid1 process from the parent's process group and session
    if (setsid() < 0) {
        die_with_error("setsid()");
    }
}

void write_file(FilePath file_path, StringView data) noexcept {
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
}

void setup_user_namespace(const sandbox::pid1::Args::LinuxNamespaces::User::Pid1& user_ns
) noexcept {
    write_file(
        "/proc/self/uid_map",
        from_unsafe{noexcept_concat(user_ns.inside_uid, ' ', user_ns.outside_uid, " 1")}
    );
    write_file("/proc/self/setgroups", "deny");
    write_file(
        "/proc/self/gid_map",
        from_unsafe{noexcept_concat(user_ns.inside_gid, ' ', user_ns.outside_gid, " 1")}
    );
    // Set real, effective and saved user ids all to the same value to prevent privilege escalation
    if (setresuid(user_ns.inside_uid, user_ns.inside_uid, user_ns.inside_uid) != 0) {
        die_with_error("setresuid()");
    }
    // Set real, effective and saved group ids all to the same value to prevent privilege escalation
    if (setresgid(user_ns.inside_gid, user_ns.inside_gid, user_ns.inside_gid) != 0) {
        die_with_error("setresgid()");
    }
}

void setup_mount_namespace(const sandbox::pid1::Args::LinuxNamespaces::Mount& mount_ns) noexcept {
    if (chdir("/")) {
        die_with_error("chdir(\"/\")");
    }
    for (const auto& oper : mount_ns.operations) {
        using Mount = sandbox::pid1::Args::LinuxNamespaces::Mount;
        std::visit(
            overloaded{
                [&](const Mount::MountTmpfs& mount_tmpfs) {
                    auto flags = MS_NOSUID | MS_SILENT;
                    if (mount_tmpfs.read_only) {
                        flags |= MS_RDONLY;
                    }
                    if (mount_tmpfs.no_exec) {
                        flags |= MS_NOEXEC;
                    }
                    auto options_str = [&]() noexcept {
                        auto size = [&]() noexcept {
                            using T =
                                decltype(mount_tmpfs.max_total_size_of_files_in_bytes)::value_type;
                            if (!mount_tmpfs.max_total_size_of_files_in_bytes) {
                                return T{0}; // no limit
                            }
                            auto x = *mount_tmpfs.max_total_size_of_files_in_bytes;
                            return x > 0 ? x : T{1}; // 1 == lowest possible limit
                        }();
                        auto nr_inodes = [&]() noexcept {
                            using T =
                                decltype(mount_tmpfs.max_total_size_of_files_in_bytes)::value_type;
                            if (!mount_tmpfs.inode_limit) {
                                return T{0}; // no limit
                            }
                            // On most systems empty tmpfs uses 1 inode, but at least on Fedora 39,
                            // it uses 2 inodes :(, so here we check for that.
                            static uint64_t empty_tmpfs_inodes = [&] {
                                if (mount(
                                        nullptr,
                                        "/",
                                        "tmpfs",
                                        MS_NOSUID | MS_SILENT | MS_NOEXEC,
                                        "size=0,nr_inodes=2,mode=0755"
                                    ))
                                {
                                    die_with_error("probing mount(tmpfs at \"/\")");
                                }
                                int res = 1;
                                if (mkdir("/../test", S_0755)) {
                                    if (errno == ENOSPC) {
                                        res = 2;
                                    } else {
                                        die_with_error("probing mkdir(\"/../test\")");
                                    }
                                }

                                if (umount2("/", MNT_DETACH)) {
                                    die_with_error("umount2()");
                                }
                                return res;
                            }();
                            // Adjust limit because root dir counts as an inode
                            uint64_t res;
                            if (__builtin_add_overflow(
                                    *mount_tmpfs.inode_limit, empty_tmpfs_inodes, &res
                                )) {
                                res = 0; // 0 == no limit
                            }
                            return res;
                        }();
                        uint8_t mode_user = (mount_tmpfs.root_dir_mode >> 6) & 7;
                        uint8_t mode_group = (mount_tmpfs.root_dir_mode >> 3) & 7;
                        uint8_t mode_other = mount_tmpfs.root_dir_mode & 7;
                        return noexcept_concat(
                            "size=",
                            size,
                            ",nr_inodes=",
                            nr_inodes,
                            ",mode=0",
                            mode_user,
                            mode_group,
                            mode_other
                        );
                    }();
                    if (mount(
                            nullptr, mount_tmpfs.path.c_str(), "tmpfs", flags, options_str.c_str()
                        )) {
                        die_with_error("mount(tmpfs at \"", mount_tmpfs.path, "\")");
                    }
                },
                [&](const Mount::MountProc& mount_proc) {
                    auto flags = MS_NOSUID | MS_SILENT;
                    if (mount_proc.read_only) {
                        flags |= MS_RDONLY;
                    }
                    if (mount_proc.no_exec) {
                        flags |= MS_NOEXEC;
                    }
                    if (mount(nullptr, mount_proc.path.c_str(), "proc", flags, nullptr)) {
                        die_with_error("mount(proc at \"", mount_proc.path, "\")");
                    }
                },
                [&](const Mount::BindMount& bind_mount) {
                    int mount_fd = open_tree(
                        AT_FDCWD,
                        bind_mount.source.c_str(),
                        OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE |
                            (bind_mount.symlink_nofollow ? AT_SYMLINK_NOFOLLOW : 0) |
                            (bind_mount.recursive ? AT_RECURSIVE : 0)
                    );
                    if (mount_fd < 0) {
                        die_with_error("open_tree(\"", bind_mount.source, "\")");
                    }

                    mount_attr mattr = {};
                    mattr.attr_set = MOUNT_ATTR_NOSUID;
                    if (bind_mount.read_only) {
                        mattr.attr_set |= MOUNT_ATTR_RDONLY;
                    }
                    if (bind_mount.no_exec) {
                        mattr.attr_set |= MOUNT_ATTR_NOEXEC;
                    }
                    if (mount_setattr(
                            mount_fd,
                            "",
                            AT_EMPTY_PATH | (bind_mount.recursive ? AT_RECURSIVE : 0),
                            &mattr,
                            sizeof(mattr)
                        ))
                    {
                        die_with_error("mount_setattr()");
                    }
                    if (move_mount(
                            mount_fd, "", AT_FDCWD, bind_mount.dest.c_str(), MOVE_MOUNT_F_EMPTY_PATH
                        ))
                    {
                        die_with_error("move_mount(dest: \"", bind_mount.dest, "\")");
                    }
                    if (close(mount_fd)) {
                        die_with_error("close()");
                    }
                },
                [&](const Mount::CreateDir& create_dir) {
                    if (mkdir(create_dir.path.c_str(), create_dir.mode)) {
                        die_with_error("mkdir(\"", create_dir.path, "\")");
                    }
                },
                [&](const Mount::CreateFile& create_file) {
                    int fd = open(
                        create_file.path.c_str(), O_CREAT | O_EXCL | O_CLOEXEC, create_file.mode
                    );
                    if (fd < 0) {
                        die_with_error("open(\"", create_file.path, "\", O_CREAT | O_EXCL)");
                    }
                    if (close(fd)) {
                        die_with_error("close()");
                    }
                },
            },
            oper
        );
    }

    if (mount_ns.new_root_mount_path) {
        if (chdir(mount_ns.new_root_mount_path->c_str())) {
            die_with_error("chdir(new_root_mount_path)");
        }
        // This has to be done within the same user namespace that performed the mount. After
        // the following clone3 with CLONE_NEWUSER | CLONE_NEWNS the whole mount tree becomes
        // locked in tracee and we really want it for security i.e. the user will not be able to
        // disintegrate part of the mount tree (but they may umount the entire mount tree).
        if (syscalls::pivot_root(".", ".")) {
            die_with_error(R"(pivot_root(".", "."))");
        }
        // Unmount the old root (also, it is needed for clone3 with CLONE_NEWUSER to succeed)
        if (umount2(".", MNT_DETACH)) {
            die_with_error(R"(umount2("."))");
        }
    }
}

void drop_all_capabilities_and_prevent_gaining_any_of_them() noexcept {
    cap_t caps = cap_init(); // all capabilities are cleared
    if (caps == nullptr) {
        die_with_error("caps_init()");
    }
    if (cap_set_proc(caps)) {
        die_with_error("cap_set_proc()");
    }
    if (cap_free(caps)) {
        die_with_error("cap_free()");
    }
}

[[nodiscard]] auto current_tracee_cpu_time_usec(int tracee_cgroup_cpu_stat_fd) noexcept {
    auto tracee_cpu_times = sandbox::supervisor::cgroups::read_cpu_times(
        tracee_cgroup_cpu_stat_fd,
        [] [[noreturn]] (auto&&... msg) noexcept {
            die_with_msg("read_cpu_times(): ", std::forward<decltype(msg)>(msg)...);
        }
    );
    return tracee_cpu_times.system_usec + tracee_cpu_times.user_usec;
}

[[nodiscard]] timespec next_cpu_timer_duration(
    uint64_t current_cpu_time_usec,
    uint64_t end_cpu_time_usec,
    double inv_max_parallelism,
    timespec min_timer_duration
) noexcept {
    auto remaining_cpu_time = end_cpu_time_usec - current_cpu_time_usec;
    auto timer_duration_usec = static_cast<long double>(remaining_cpu_time) * inv_max_parallelism;
    long double timer_duration_sec;
    long double timer_duration_nsec =
        modfl(timer_duration_usec / 1'000'000, &timer_duration_sec) * 1e9;
    timespec timer_duration = {
        .tv_sec = static_cast<decltype(timespec::tv_sec)>(timer_duration_sec),
        .tv_nsec = static_cast<decltype(timespec::tv_nsec)>(timer_duration_nsec),
    };
    if (timer_duration < min_timer_duration) {
        return min_timer_duration;
    }
    return timer_duration;
}

struct SignalHandlersState {
    struct RealTime {
        bool is;
        timer_t timer_id;
        timespec limit;
    } real_time;

    struct CpuTime {
        bool is;
        timer_t timer_id;
        timespec limit;
        double inv_max_tracee_parallelism;
        int tracee_cgroup_cpu_stat_fd;
        uint64_t timer_start_cpu_time_usec;
        uint64_t end_cpu_time_usec;
    } cpu_time;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile SignalHandlersState* signal_handlers_state_ptr = nullptr;

void signal_handler_to_kill_the_tracee(int /*sig*/, siginfo_t* si, void* /*ucontext*/) noexcept {
    if (si->si_code == SI_TIMER) {
        int saved_errno = errno;
        // Kill the tracee
        int tracee_cgroup_kill_fd = si->si_int;
        if (write(tracee_cgroup_kill_fd, "1", 1) < 0) {
            die_with_error("write()");
        }
        // Restore errno
        errno = saved_errno;
    }
}

void set_up_real_time_timer() noexcept {
    if (!signal_handlers_state_ptr->real_time.is) {
        return;
    }
    // Start the timer
    itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value =
            {
                .tv_sec = signal_handlers_state_ptr->real_time.limit.tv_sec,
                .tv_nsec = signal_handlers_state_ptr->real_time.limit.tv_nsec,
            },
    };
    if (its.it_value == timespec{.tv_sec = 0, .tv_nsec = 0}) {
        // it_value == 0 disables the timer, but the meaning of time limit == 0 is to
        // provide no time for tracee, so we use the minimal viable time limit value
        its.it_value = {.tv_sec = 0, .tv_nsec = 1};
    }
    if (timer_settime(signal_handlers_state_ptr->real_time.timer_id, 0, &its, nullptr)) {
        die_with_error("timer_settime()");
    }
}

void sigxcpu_handler_to_kill_tracee_upon_cpu_time_limit(
    int /*sig*/, siginfo_t* si, void* /*ucontext*/
) noexcept {
    if (si->si_code == SI_TIMER) {
        int saved_errno = errno;

        auto curr_cpu_time_usec = current_tracee_cpu_time_usec(
            signal_handlers_state_ptr->cpu_time.tracee_cgroup_cpu_stat_fd
        );
        auto end_cpu_time_usec = signal_handlers_state_ptr->cpu_time.end_cpu_time_usec;
        if (curr_cpu_time_usec >= end_cpu_time_usec) {
            // Kill the tracee
            int tracee_cgroup_kill_fd = si->si_int;
            if (write(tracee_cgroup_kill_fd, "1", 1) < 0) {
                die_with_error("write()");
            }
        } else {
            // Restart the timer
            itimerspec its = {
                .it_interval = {.tv_sec = 0, .tv_nsec = 0},
                .it_value = next_cpu_timer_duration(
                    curr_cpu_time_usec,
                    end_cpu_time_usec,
                    signal_handlers_state_ptr->cpu_time.inv_max_tracee_parallelism,
                    timespec{.tv_sec = 0, .tv_nsec = 1'000'000} // = max 1000 timer expirations per
                                                                // second
                ),
            };
            if (timer_settime(signal_handlers_state_ptr->cpu_time.timer_id, 0, &its, nullptr)) {
                die_with_error("timer_settime()");
            }
        }

        // Restore errno
        errno = saved_errno;
    }
}

void set_up_cpu_time_timer() noexcept {
    if (!signal_handlers_state_ptr->cpu_time.is) {
        return;
    }
    // Cast to volatile, as we are in the signal handler
    volatile auto volatile_sms = shared_mem_state;
    auto start_cpu_time_usec = volatile_sms->tracee_exec_start_cpu_time_user.usec +
        volatile_sms->tracee_exec_start_cpu_time_system.usec;
    signal_handlers_state_ptr->cpu_time.timer_start_cpu_time_usec = start_cpu_time_usec;

    decltype(signal_handlers_state_ptr->cpu_time.end_cpu_time_usec) end_cpu_time_usec;
    if (__builtin_mul_overflow(
            signal_handlers_state_ptr->cpu_time.limit.tv_sec, 1'000'000, &end_cpu_time_usec
        ))
    {
        die_with_msg("cpu time limit is too big");
    }
    if (__builtin_add_overflow(
            end_cpu_time_usec,
            (signal_handlers_state_ptr->cpu_time.limit.tv_nsec + 999) /
                1000, // round up nanoseconds to microseconds
            &end_cpu_time_usec
        ))
    {
        die_with_msg("cpu time limit is too big");
    }
    if (__builtin_add_overflow(end_cpu_time_usec, start_cpu_time_usec, &end_cpu_time_usec)) {
        die_with_msg("cpu time limit is too big");
    }
    signal_handlers_state_ptr->cpu_time.end_cpu_time_usec = end_cpu_time_usec;
    // Start the timer
    itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value = next_cpu_timer_duration(
            start_cpu_time_usec,
            end_cpu_time_usec,
            signal_handlers_state_ptr->cpu_time.inv_max_tracee_parallelism,
            // duration == 0 disables the timer and it is not what we want
            {.tv_sec = 0, .tv_nsec = 1}
        ),
    };
    if (timer_settime(signal_handlers_state_ptr->cpu_time.timer_id, 0, &its, nullptr)) {
        die_with_error("timer_settime()");
    }
}

void sigusr2_handler_to_start_timer(int /*sig*/, siginfo_t* si, void* /*ucontext*/) noexcept {
    if (si->si_code == SI_USER && si->si_pid > 0) { // signal came from the tracee process
        int saved_errno = errno;
        set_up_real_time_timer();
        set_up_cpu_time_timer();
        // Disable this signal handler
        struct sigaction sa = {};
        sa.sa_handler = SIG_IGN;
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGUSR2, &sa, nullptr)) {
            die_with_error("sigaction()");
        }
        // Restore errno
        errno = saved_errno;
    }
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SignalHandlersState signal_handlers_state;

struct InstallSignalHandlersForTimeLimitsArgs {
    int tracee_cgroup_kill_fd;
    int tracee_cgroup_cpu_stat_fd;
    optional<timespec> real_time_limit;
    optional<timespec> cpu_time_limit;
    double max_tracee_parallelism;
};

void install_signal_handlers_for_time_limits(InstallSignalHandlersForTimeLimitsArgs&& args
) noexcept {
    if (!args.real_time_limit && !args.cpu_time_limit) {
        // SIGUSR2 from tracee will be ignored because this is the process with PID = 1 in the
        // current pid namespace
        return;
    }

    if (args.real_time_limit) {
        // Create the timer
        sigevent sev = {};
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGUSR1;
        sev.sigev_value.sival_int = args.tracee_cgroup_kill_fd;
        timer_t timer_id;
        if (timer_create(CLOCK_MONOTONIC, &sev, &timer_id)) {
            die_with_error("timer_create()");
        }

        signal_handlers_state.real_time = {
            .is = true,
            .timer_id = timer_id,
            .limit = *args.real_time_limit,
        };

        // Install signal handler for SIGUSR1
        struct sigaction sa = {};
        sa.sa_sigaction = &signal_handler_to_kill_the_tracee;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        if (sigaction(SIGUSR1, &sa, nullptr)) {
            die_with_error("sigaction()");
        }
    } else {
        signal_handlers_state.real_time.is = false;
    }

    if (args.cpu_time_limit) {
        // Create the timer
        sigevent sev = {};
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGXCPU;
        sev.sigev_value.sival_int = args.tracee_cgroup_kill_fd;
        timer_t timer_id;
        if (timer_create(CLOCK_MONOTONIC, &sev, &timer_id)) {
            die_with_error("timer_create()");
        }

        signal_handlers_state.cpu_time = {
            .is = true,
            .timer_id = timer_id,
            .limit = *args.cpu_time_limit,
            .inv_max_tracee_parallelism = 1 / args.max_tracee_parallelism,
            .tracee_cgroup_cpu_stat_fd = args.tracee_cgroup_cpu_stat_fd,
            .timer_start_cpu_time_usec = 0,
            .end_cpu_time_usec = 0,
        };

        // Install signal handler for SIGXCPU
        struct sigaction sa = {};
        sa.sa_sigaction = &sigxcpu_handler_to_kill_tracee_upon_cpu_time_limit;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        if (sigaction(SIGXCPU, &sa, nullptr)) {
            die_with_error("sigaction()");
        }
    } else {
        signal_handlers_state.cpu_time.is = false;
    }

    signal_handlers_state_ptr = &signal_handlers_state;
    // Install signal handler for SIGUSR2
    struct sigaction sa = {};
    sa.sa_sigaction = &sigusr2_handler_to_start_timer;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGUSR2, &sa, nullptr)) {
        die_with_error("sigaction()");
    }
}

struct Pipe {
    int read_fd;
    int write_fd;
};

Pipe create_pipe_to_signal_to_setup_tracee_cpu_timer() noexcept {
    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC)) {
        die_with_error("pipe2()");
    }
    return Pipe{
        .read_fd = pfd[0],
        .write_fd = pfd[1],
    };
}

void wait_for_signal_to_setup_tracee_cpu_timer_and_setup_the_timer(
    const Pipe& pipe, pid_t tracee_pid, int tracee_cgroup_kill_fd, timespec cpu_time_limit
) noexcept {
    if (close(pipe.write_fd)) {
        die_with_error("close()");
    }
    char buff;
    if (read(pipe.read_fd, &buff, sizeof(buff)) < 0) {
        die_with_error("read()");
    }
    clockid_t tracee_cpu_clock_id;
    // This function cannot be run inside signal handler
    if (clock_getcpuclockid(tracee_pid, &tracee_cpu_clock_id)) {
        if (errno == ESRCH) { // tracee died
            return;
        }
        die_with_error("clock_getcpuclockid()");
    }
    // Create the timer
    sigevent sev = {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGXCPU;
    sev.sigev_value.sival_int = tracee_cgroup_kill_fd;
    timer_t timer_id;
    // This function cannot be run inside signal handler
    if (timer_create(tracee_cpu_clock_id, &sev, &timer_id)) {
        die_with_error("timer_create()");
    }
    // Install signal handler for SIGXCPU
    struct sigaction sa = {};
    sa.sa_sigaction = &signal_handler_to_kill_the_tracee;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGXCPU, &sa, nullptr)) {
        die_with_error("sigaction()");
    }
    // Start the timer
    itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value = cpu_time_limit,
    };
    if (its.it_value == timespec{.tv_sec = 0, .tv_nsec = 0}) {
        // it_value == 0 disables the timer and it is not what we want, so we use the lowest viable
        // value
        its.it_value = {.tv_sec = 0, .tv_nsec = 1};
    }
    if (timer_settime(timer_id, 0, &its, nullptr)) {
        die_with_error("timer_settime()");
    }
}

template <size_t N>
void close_all_non_std_file_descriptors_except(int (&&surviving_fds)[N]) noexcept {
    static_assert(N > 0);
    std::sort(std::begin(surviving_fds), std::end(surviving_fds));
    auto prev_fd = STDERR_FILENO;
    for (auto fd : surviving_fds) {
        if (prev_fd + 1 < fd && close_range(prev_fd + 1, fd - 1, 0)) {
            die_with_error("close_range()");
        }
        prev_fd = fd;
    }
    // Close all remaining fds
    if (close_range(prev_fd + 1, ~0U, 0)) {
        die_with_error("close_range()");
    }
}

void harden_against_potential_compromise(sock_fprog seccomp_filter) noexcept {
    // Cut access to cgroups other than ours
    unshare(CLONE_NEWCGROUP);

    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &seccomp_filter)) {
        die_with_error("seccomp()");
    }
}

timespec get_current_time() noexcept {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
        die_with_error("clock_gettime()");
    }
    return ts;
}

} // namespace

namespace sandbox::pid1 {

[[noreturn]] void main(Args args) noexcept {
    shared_mem_state = args.shared_mem_state;

    set_process_name();
    setup_kill_on_supervisor_death(args.supervisor_pidfd);
    setup_session_and_process_group();
    setup_user_namespace(args.linux_namespaces.user.pid1);
    int proc_dirfd = open("/proc", O_RDONLY | O_CLOEXEC);
    setup_mount_namespace(args.linux_namespaces.mount);
    drop_all_capabilities_and_prevent_gaining_any_of_them();
    install_signal_handlers_for_time_limits({
        .tracee_cgroup_kill_fd = args.tracee_cgroup_kill_fd,
        .tracee_cgroup_cpu_stat_fd = args.tracee_cgroup_cpu_stat_fd,
        .real_time_limit = args.time_limit,
        .cpu_time_limit =
            args.tracee_is_restricted_to_single_thread ? std::nullopt : args.cpu_time_limit,
        .max_tracee_parallelism = args.max_tracee_parallelism,
    });
    auto pipe_to_signal_to_setup_tracee_cpu_timer =
        args.tracee_is_restricted_to_single_thread && args.cpu_time_limit
        ? optional{create_pipe_to_signal_to_setup_tracee_cpu_timer()}
        : std::nullopt;

    if (UNDEFINED_SANITIZER) {
        auto ignore_signal = [&](int sig) noexcept {
            struct sigaction sa = {};
            sa.sa_handler = SIG_DFL;
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
    // CLONE_NEWUSER | CLONE_NEWNS are needed to lock the mount tree
    cl_args.flags = CLONE_NEWCGROUP | CLONE_INTO_CGROUP | CLONE_NEWUSER | CLONE_NEWNS;
    cl_args.exit_signal = SIGCHLD;
    cl_args.cgroup = static_cast<uint64_t>(args.tracee_cgroup_fd);

    auto tracee_pid = syscalls::clone3(&cl_args);
    if (tracee_pid == -1) {
        die_with_error("clone3()");
    }
    if (tracee_pid == 0) {
        tracee::main({
            .shared_mem_state = args.shared_mem_state,
            .executable = args.executable,
            .stdin_fd = args.stdin_fd,
            .stdout_fd = args.stdout_fd,
            .stderr_fd = args.stderr_fd,
            .argv = std::move(args.argv),
            .env = std::move(args.env),
            .proc_dirfd = proc_dirfd,
            .tracee_cgroup_cpu_stat_fd = args.tracee_cgroup_cpu_stat_fd,
            .signal_pid1_to_setup_tracee_cpu_timer_fd = pipe_to_signal_to_setup_tracee_cpu_timer
                ? optional{pipe_to_signal_to_setup_tracee_cpu_timer->write_fd}
                : std::nullopt,

            .fd_to_close_upon_execve = args.fd_to_close_upon_execve,
            .linux_namespaces =
                {
                    .user =
                        {
                            .outside_uid = args.linux_namespaces.user.tracee.outside_uid,
                            .inside_uid = args.linux_namespaces.user.tracee.inside_uid,
                            .outside_gid = args.linux_namespaces.user.tracee.outside_gid,
                            .inside_gid = args.linux_namespaces.user.tracee.inside_gid,
                        },
                },
            .prlimit = args.prlimit,
            .seccomp_bpf_fd = args.tracee_seccomp_bpf_fd,
        });
    }

    if (pipe_to_signal_to_setup_tracee_cpu_timer) {
        if (!args.cpu_time_limit) {
            die_with_msg("BUG: args.cpu_time_limit == false");
        }
        wait_for_signal_to_setup_tracee_cpu_timer_and_setup_the_timer(
            *pipe_to_signal_to_setup_tracee_cpu_timer,
            static_cast<pid_t>(tracee_pid),
            args.tracee_cgroup_kill_fd,
            *args.cpu_time_limit
        );
    }

    close_all_non_std_file_descriptors_except(
        {args.tracee_cgroup_kill_fd, args.tracee_cgroup_cpu_stat_fd}
    );
    harden_against_potential_compromise(args.seccomp_filter);

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
