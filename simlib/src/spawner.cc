#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <simlib/call_in_destructor.hh>
#include <simlib/concat.hh>
#include <simlib/directory.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/meta/is_one_of.hh>
#include <simlib/overloaded.hh>
#include <simlib/spawner.hh>
#include <simlib/string_transform.hh>
#include <simlib/syscalls.hh>
#include <simlib/time.hh>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

using std::array;
using std::string;
using std::vector;

string Spawner::receive_error_message(const siginfo_t& si, int fd) {
    STACK_UNWINDING_MARK;

    string message;
    array<char, 4096> buff{};
    // Read errors from fd
    ssize_t rc = 0;
    while ((rc = read(fd, buff.data(), buff.size())) > 0) {
        message.append(buff.data(), rc);
    }

    if (!message.empty()) { // Error in tracee
        THROW(message);
    }

    switch (si.si_code) {
    case CLD_EXITED: message = concat_tostr("exited with ", si.si_status); break;
    case CLD_KILLED:
        message = concat_tostr("killed by signal ", si.si_status, " - ", strsignal(si.si_status));
        break;
    case CLD_DUMPED:
        message = concat_tostr(
            "killed and dumped by signal ", si.si_status, " - ", strsignal(si.si_status)
        );
        break;
    default: THROW("Invalid siginfo_t.si_code: ", si.si_code);
    }

    return message;
}

timespec Spawner::Timer::delete_timer_and_get_remaning_time() noexcept {
    return std::visit(
        overloaded{
            [](const WithoutTimeout& /*unused*/) {
                return timespec{0, 0};
            },
            [&](WithTimeout& state) {
                if (not state.timer_is_active) {
                    return timespec{0, 0};
                }
                state.timer_is_active = false;
                // Disarm timer and check if it has
                // expired
                itimerspec new_its{{0, 0}, {0, 0}};
                itimerspec old_its{};
                int rc = timer_settime(state.timer_id, 0, &new_its, &old_its);
                assert(rc == 0);
                if (old_its.it_value == timespec{0, 0}) {
                    // timer is already disarmed => the
                    // signal handler was / is about to
                    // run => wait for it
                    while (not timeout_signal_was_sent()) {
                        pause();
                    }
                }

                rc = timer_delete(state.timer_id);
                assert(rc == 0);
                return old_its.it_value;
            }},
        state_
    );
}

Spawner::Timer::Timer(
    pid_t watched_pid,
    std::chrono::nanoseconds time_limit,
    clockid_t clock_id,
    int timeout_signal,
    int timer_signal
)
: clock_id_(clock_id)
, creator_thread_id_(syscalls::gettid())
, state_([&]() -> decltype(state_) {
    STACK_UNWINDING_MARK;
    assert(time_limit >= decltype(time_limit)::zero());
    if (time_limit == std::chrono::nanoseconds::zero()) {
        timespec curr_clock_time{};
        if (clock_gettime(clock_id, &curr_clock_time)) {
            THROW("clock_gettime()", errmsg());
        }
        return WithoutTimeout{curr_clock_time};
    }

    return WithTimeout{to_timespec(time_limit), {}, false, {watched_pid, timeout_signal, false}};
}()) {

    if (std::holds_alternative<WithoutTimeout>(state_)) {
        return; // Nothing more to do
    }

    auto& state = std::get<WithTimeout>(state_);
    // It is OK to use static, since the class and constructor is not a template
    static constexpr auto timeout_handler = [](int /*unused*/,
                                               siginfo_t* si,
                                               void* /*unused*/) noexcept {
        if (si->si_code != SI_TIMER) {
            return; // Ignore other signals
        }

        int errnum = errno;
        SignalHandlerContext& context = *static_cast<SignalHandlerContext*>(si->si_value.sival_ptr);
        (void)kill(context.watched_pid, context.timeout_signal); // signal safe
        context.timeout_signal_was_sent = true;
        errno = errnum;
    };

    // Install timeout signal handler
    struct sigaction sa = {};
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timeout_handler;
    if (sigaction(timer_signal, &sa, nullptr)) {
        THROW("sigaction()", errmsg());
    }

    // Prepare timer
    sigevent sev{};
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev._sigev_un._tid = syscalls::gettid(); // sigev_notify_thread_id
    sev.sigev_signo = timer_signal;
    sev.sigev_value.sival_ptr = &state.signal_handler_context;
    if (timer_create(clock_id, &sev, &state.timer_id)) {
        THROW("timer_create()", errmsg());
    }

    state.timer_is_active = true;

    // Arm timer
    itimerspec its{{0, 0}, state.time_limit};
    if (timer_settime(state.timer_id, 0, &its, nullptr)) {
        int errnum = errno;
        (void)delete_timer_and_get_remaning_time();
        THROW("timer_settime()", errmsg(errnum));
    }
}

std::chrono::nanoseconds Spawner::Timer::deactivate_and_get_runtime() noexcept {
    return std::visit(
        overloaded{
            [&](const WithoutTimeout& state) {
                timespec curr_clock_time{};
                int rc = clock_gettime(clock_id_, &curr_clock_time);
                assert(rc == 0);
                return to_nanoseconds(curr_clock_time - state.start_clock_time);
            },
            [&](WithTimeout& state) {
                assert(state.timer_is_active and "You can call this function only once");
                return to_nanoseconds(state.time_limit - delete_timer_and_get_remaning_time());
            }},
        state_
    );
}

bool Spawner::Timer::timeout_signal_was_sent() const noexcept {
    // In case when other thread calls it, we may not detect this in the below
    // assert every time due to the memory ordering phenomenon, but it should be
    // sufficient. Atomic is of no help here, as the other thread might see see
    // value of the atomic that was there before creation of the atomic
    // itself...
    assert(
        creator_thread_id_ == syscalls::gettid() and
        "This can only be used by the same thread that constructed "
        "this object"
    );
    return std::visit(
        overloaded{
            [&](const WithoutTimeout& /*unused*/) { return false; },
            [&](const WithTimeout& state) -> bool {
                return state.signal_handler_context.timeout_signal_was_sent;
            }},
        state_
    );
}

Spawner::ExitStat Spawner::run(
    FilePath exec,
    const vector<string>& exec_args,
    const Spawner::Options& opts,
    const std::function<void(pid_t)>& do_in_parent_after_fork
) {
    STACK_UNWINDING_MARK;

    using std::chrono_literals::operator""ns;

    if (opts.real_time_limit.has_value() and opts.real_time_limit.value() <= 0ns) {
        THROW("If set, real_time_limit has to be greater than 0");
    }

    if (opts.cpu_time_limit.has_value() and opts.cpu_time_limit.value() <= 0ns) {
        THROW("If set, cpu_time_limit has to be greater than 0");
    }

    if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0) {
        THROW("If set, memory_limit has to be greater than 0");
    }

    // Error stream from child via pipe
    array<int, 2> pfd{};
    if (pipe2(pfd.data(), O_CLOEXEC) == -1) {
        THROW("pipe()", errmsg());
    }

    int cpid = fork();
    if (cpid == -1) {
        THROW("fork()", errmsg());
    }
    if (cpid == 0) {
        close(pfd[0]);
        run_child(exec, exec_args, opts, pfd[1], [] {});
    }

    close(pfd[1]);
    FileDescriptor pipe0_closer(pfd[0]);

    // Wait for child to be ready
    siginfo_t si;
    rusage ru{};
    if (syscalls::waitid(P_PID, cpid, &si, WSTOPPED | WEXITED, &ru) == -1) {
        THROW("waitid()", errmsg());
    }

    // If something went wrong
    if (si.si_code != CLD_STOPPED) {
        return {0ns, 0ns, si.si_code, si.si_status, ru, 0, receive_error_message(si, pfd[0])};
    }

    // Useful when exception is thrown
    CallInDtor kill_and_wait_child_guard([&] {
        kill(-cpid, SIGKILL);
        syscalls::waitid(P_PID, cpid, &si, WEXITED, nullptr);
    });

    do_in_parent_after_fork(cpid);

    clockid_t child_cpu_clock_id = 0;
    if (clock_getcpuclockid(cpid, &child_cpu_clock_id)) {
        THROW("clock_getcpuclockid()", errmsg());
    }

    // Set up timers
    Timer timer(cpid, opts.real_time_limit.value_or(0ns), CLOCK_MONOTONIC);
    Timer cpu_timer(cpid, opts.cpu_time_limit.value_or(0ns), child_cpu_clock_id);
    kill(cpid, SIGCONT); // There is only one process now, so '-' is not needed

    // Wait for death of the child
    syscalls::waitid(P_PID, cpid, &si, WEXITED | WNOWAIT, nullptr);

    // Get runtime and cpu runtime
    auto runtime = timer.deactivate_and_get_runtime();
    auto cpu_runtime = cpu_timer.deactivate_and_get_runtime();

    kill_and_wait_child_guard.cancel();
    syscalls::waitid(P_PID, cpid, &si, WEXITED, &ru);

    if (si.si_code != CLD_EXITED or si.si_status != 0) {
        return {
            runtime,
            cpu_runtime,
            si.si_code,
            si.si_status,
            ru,
            0,
            receive_error_message(si, pfd[0])};
    }

    return {runtime, cpu_runtime, si.si_code, si.si_status, ru, 0};
}

void Spawner::run_child(
    FilePath exec,
    const std::vector<std::string>& exec_args,
    const Options& opts,
    int fd,
    const std::function<void()>& do_before_exec
) noexcept {
    STACK_UNWINDING_MARK;
    // Sends error to parent
    auto send_error_and_exit = [fd](int errnum, CStringView str) {
        send_error_message_and_exit(fd, errnum, str);
    };

    // Create new process group (useful for killing the whole process group)
    if (setpgid(0, 0)) {
        send_error_and_exit(errno, "setpgid()");
    }

    using std::chrono_literals::operator""ns;

    if (opts.real_time_limit.has_value() and opts.real_time_limit.value() <= 0ns) {
        send_error_message_and_exit(fd, "If set, real_time_limit has to be greater than 0");
    }

    if (opts.cpu_time_limit.has_value() and opts.cpu_time_limit.value() <= 0ns) {
        send_error_message_and_exit(fd, "If set, cpu_time_limit has to be greater than 0");
    }

    if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0) {
        send_error_message_and_exit(fd, "If set, memory_limit has to be greater than 0");
    }

    // Convert exec_args
    const size_t len = exec_args.size();
    std::unique_ptr<const char*[]> args(new (std::nothrow) const char*[len + 1]);
    if (not args) {
        send_error_message_and_exit(fd, "Out of memory");
    }

    args[len] = nullptr;
    for (size_t i = 0; i < len; ++i) {
        args[i] = exec_args[i].c_str();
    }

    // Change working directory
    if (not is_one_of(opts.working_dir, "", ".", "./")) {
        if (chdir(opts.working_dir.c_str()) == -1) {
            send_error_and_exit(errno, "chdir()");
        }
    }

    // Set virtual memory and stack size limit (to the same value)
    if (opts.memory_limit.has_value()) {
        struct rlimit limit = {};
        limit.rlim_max = limit.rlim_cur = opts.memory_limit.value();
        if (setrlimit(RLIMIT_AS, &limit)) {
            send_error_and_exit(errno, "setrlimit(RLIMIT_AS)");
        }
        if (setrlimit(RLIMIT_STACK, &limit)) {
            send_error_and_exit(errno, "setrlimit(RLIMIT_STACK)");
        }
    }

    using std::chrono_literals::operator""ns;
    using std::chrono_literals::operator""s;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    auto set_cpu_rlimit = [&](nanoseconds cpu_tl) {
        // Limit below is useful when spawned process becomes orphaned
        rlimit limit{};
        limit.rlim_cur = limit.rlim_max =
            duration_cast<seconds>(cpu_tl + 1.5s).count(); // + 1.5 to avoid premature death

        if (setrlimit(RLIMIT_CPU, &limit)) {
            send_error_and_exit(errno, "setrlimit(RLIMIT_CPU)");
        }
    };

    // Set CPU time limit [s]
    if (opts.cpu_time_limit.has_value()) {
        set_cpu_rlimit(opts.cpu_time_limit.value());
    } else if (opts.real_time_limit.has_value()) {
        set_cpu_rlimit(opts.real_time_limit.value());
    }

    // Change stdin
    if (opts.new_stdin_fd < 0) {
        close(STDIN_FILENO);
    } else if (opts.new_stdin_fd != STDIN_FILENO) {
        while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1) {
            if (errno != EINTR) {
                send_error_and_exit(errno, "dup2()");
            }
        }
    }

    // Change stdout
    if (opts.new_stdout_fd < 0) {
        close(STDOUT_FILENO);
    } else if (opts.new_stdout_fd != STDOUT_FILENO) {
        while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1) {
            if (errno != EINTR) {
                send_error_and_exit(errno, "dup2()");
            }
        }
    }

    // Change stderr
    if (opts.new_stderr_fd < 0) {
        close(STDERR_FILENO);
    } else if (opts.new_stderr_fd != STDERR_FILENO) {
        while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1) {
            if (errno != EINTR) {
                send_error_and_exit(errno, "dup2()");
            }
        }
    }

    // Close file descriptors that are not needed to be open (for security
    // reasons)
    {
        Directory dir("/proc/thread-self/fd");
        if (dir == nullptr) {
            send_error_and_exit(errno, "opendir()");
        }

        array permitted_fds = {
            dirfd(dir),
            fd, // Needed in case of errors (it has FD_CLOEXEC flag set)
            (opts.new_stdin_fd < 0 ? fd : STDIN_FILENO),
            (opts.new_stdout_fd < 0 ? fd : STDOUT_FILENO),
            (opts.new_stderr_fd < 0 ? fd : STDERR_FILENO),
        };

        for_each_dir_component(
            dir,
            [&](dirent* file) {
                auto filename = str2num<int>(file->d_name);
                if (filename) {
                    for (int fd_no : permitted_fds) {
                        if (*filename == fd_no) {
                            return;
                        }
                    }
                }

                if (auto opt = str2num<int>(file->d_name); opt) {
                    close(*opt);
                }
            },
            [&] { send_error_and_exit(errno, "readdir()"); }
        );
    }

    try {
        do_before_exec();
    } catch (std::exception& e) {
        send_error_message_and_exit(fd, CStringView(e.what()));
    }

    // Signal parent process that child is ready to execute @p exec
    kill(getpid(), SIGSTOP);

    execvp(exec, const_cast<char* const*>(args.get()));
    int errnum = errno;

    // execvp() failed
    if (exec.size() <= PATH_MAX) {
        send_error_and_exit(
            errnum, intentional_unsafe_cstring_view(concat<PATH_MAX + 20>("execvp('", exec, "')"))
        );
    } else {
        send_error_and_exit(
            errnum,
            intentional_unsafe_cstring_view(
                concat<PATH_MAX + 20>("execvp('", exec.to_cstr().substring(0, PATH_MAX), "...')")
            )
        );
    }
}
