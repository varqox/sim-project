#include "client/request/serialize.hh"
#include "communication/client_supervisor.hh"
#include "do_die_with_error.hh"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <optional>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_perms.hh>
#include <simlib/macros/throw.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/pipe.hh>
#include <simlib/random.hh>
#include <simlib/read_bytes_as.hh>
#include <simlib/read_exact.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/socket_send_fds.hh>
#include <simlib/socket_stream_ext.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/to_string.hh>
#include <simlib/utilities.hh>
#include <simlib/write_as_bytes.hh>
#include <string>
#include <string_view>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

using std::optional;

extern "C" {
extern const unsigned char sandbox_supervisor_dump[];
extern const unsigned int sandbox_supervisor_dump_len;
} // extern "C"

static void write_file(int error_fd, FilePath file_path, StringView data) noexcept {
    auto fd = open(file_path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd == -1) {
        sandbox::do_die_with_error(error_fd, "open(", file_path, ")");
    }
    if (write_all(fd, data) != data.size()) {
        sandbox::do_die_with_error(error_fd, "write(", file_path, ")");
    }
    if (close(fd)) {
        sandbox::do_die_with_error(error_fd, "close()");
    }
}

namespace sandbox {

[[noreturn]] void execute_supervisor(int error_fd, int sock_fd) noexcept {
    /* Set up file descriptors:
     * error_fd will become STDERR_FILENO
     * sock_fd need to be greater than STDERR_FILENO
     */
    if (sock_fd <= STDERR_FILENO) {
        // The old sock_fd will be closed either by exec() or by duplicating error_fd into
        // STDERR_FILENO
        sock_fd = fcntl(
            sock_fd,
            F_DUPFD,
            STDERR_FILENO + 1
        ); // without CLOEXEC - it must survive exec()
        if (sock_fd == -1) {
            do_die_with_error(error_fd, "fcntl()");
        }
    } else {
        // Unset CLOEXEC on sock_fd. This is safe in the child process, as there is no other thread
        // that could call fork() while the file descriptor has the CLOEXEC flag unset
        if (fcntl(sock_fd, F_SETFD, 0)) {
            do_die_with_error(error_fd, "fcntl()");
        }
    }
    // Supervisor will write all errors to STDERR_FILENO and we want to capture them to error_fd
    if (error_fd == STDERR_FILENO) {
        // Unset CLOEXEC on error_fd. This is safe in the child process, as there is no other thread
        // that could call fork() while the file descriptor has the CLOEXEC flag unset
        if (fcntl(error_fd, F_SETFD, 0)) {
            do_die_with_error(error_fd, "fcntl()");
        }
    } else {
        if (dup2(error_fd, STDERR_FILENO) == -1) {
            do_die_with_error(error_fd, "dup2()");
        }
    }

    // Write the supervisor executable to a memfd without CLOEXEC - it must survive the exec()
    // as systemd-run has to execute it
    // NOLINTNEXTLINE(android-cloexec-memfd-create)
    int supervisor_exe_fd = memfd_create("supervisor executable", 0);
    if (supervisor_exe_fd == -1) {
        do_die_with_error(error_fd, "memfd_create()");
    }
    if (write_all(supervisor_exe_fd, sandbox_supervisor_dump, sandbox_supervisor_dump_len) !=
        sandbox_supervisor_dump_len)
    {
        do_die_with_error(error_fd, "write()");
    }

    static constexpr auto cgroup_fs_path = std::string_view{"/sys/fs/cgroup"};
    auto cgroup_path = [&] {
        try {
            std::string str = get_file_contents("/proc/self/cgroup");
            auto pos = str.find(':');
            if (pos == std::string::npos) {
                do_die_with_msg(error_fd, "invalid contents of file: /proc/self/cgorup");
            }
            pos = str.find(':', pos + 1);
            if (pos == std::string::npos) {
                do_die_with_msg(error_fd, "invalid contents of file: /proc/self/cgorup");
            }
            str.erase(0, pos + 1); // Remove prefix
            str.insert(0, cgroup_fs_path);
            while (str.back() == '\n' || str.back() == '/') {
                str.pop_back();
            }
            return str;
        } catch (const std::exception& e) {
            do_die_with_msg(error_fd, "exception: ", e.what());
        }
    }();

    auto get_file_owner = [error_fd](const char* cgroup_path) {
        struct stat64 st;
        if (stat64(cgroup_path, &st)) {
            do_die_with_error(error_fd, "stat64()");
        }
        return st.st_uid;
    };

    auto euid = geteuid();
    if (get_file_owner(cgroup_path.c_str()) == euid) {
        // Find top-most cgroup that we are owner of
        size_t len = cgroup_path.size();
        while (len > cgroup_fs_path.size()) {
            size_t saved_len = len;
            --len;
            while (cgroup_path[len] != '/') {
                --len;
            }
            cgroup_path[len] = '\0';
            if (get_file_owner(cgroup_path.c_str()) != euid) {
                cgroup_path[len] = '/';
                len = saved_len;
                break;
            }
        }
        cgroup_path.resize(len);

        constexpr size_t new_cgroup_name_len = 16;
        try {
            cgroup_path += '/';
            cgroup_path.resize(cgroup_path.size() + new_cgroup_name_len);
        } catch (const std::exception& e) {
            do_die_with_msg(error_fd, "exception: ", e.what());
        }

        // Try creating a new cgroup
        for (;;) {
            for (size_t i = cgroup_path.size() - new_cgroup_name_len; i < cgroup_path.size(); ++i) {
                cgroup_path[i] = get_random('0', '9');
            }
            if (mkdir(cgroup_path.c_str(), S_0755)) {
                if (errno == EEXIST) {
                    continue; // try with different name
                }
                if (errno == EACCES) {
                    break; // run supervisor with systemd-run
                }
                do_die_with_error(error_fd, "mkdir()");
            }

            /* Successfully created cgroup, move the current process to the created cgroup and
             * remaining processes to an other cgroup */

            try {
                cgroup_path += "/cgroup.procs";
            } catch (const std::exception& e) {
                do_die_with_msg(error_fd, "exception: ", e.what());
            }
            int new_cgroup_procs_fd = open(cgroup_path.c_str(), O_WRONLY | O_CLOEXEC);
            if (new_cgroup_procs_fd == 1) {
                do_die_with_error(error_fd, "open()");
            }

            cgroup_path.resize(
                cgroup_path.size() - new_cgroup_name_len - std::strlen("/cgroup.procs")
            );
            cgroup_path += "cgroup.procs"; // won't throw
            auto cgroup_procs_fd = open(cgroup_path.c_str(), O_RDONLY | O_CLOEXEC);
            if (cgroup_procs_fd == -1) {
                do_die_with_error(error_fd, "open()");
            }

            // Acquire the lock to prevent race conditions
            for (;;) {
                if (!flock(cgroup_procs_fd, LOCK_EX)) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                do_die_with_error(error_fd, "flock()");
            }

            // Move the current proces to the created cgroup
            if (write(new_cgroup_procs_fd, "0", 1) != 1) {
                do_die_with_error(error_fd, "write()");
            }

            // Create "others" cgroup
            cgroup_path.resize(cgroup_path.size() - std::strlen("cgroup.procs"));
            cgroup_path += "others"; // won't throw
            if (mkdir(cgroup_path.c_str(), S_0755) && errno != EEXIST) {
                do_die_with_error(error_fd, "mkdir()");
            }

            try {
                cgroup_path += "/cgroup.procs";
            } catch (const std::exception& e) {
                do_die_with_msg(error_fd, "exception: ", e.what());
            }
            auto others_cgroup_procs_fd = open(cgroup_path.c_str(), O_WRONLY | O_CLOEXEC);
            if (others_cgroup_procs_fd == -1) {
                do_die_with_error(error_fd, "open()");
            }

            std::array<char, sizeof(pid_t) * 3 /*3 digits for one byte*/ + 1 /*newline*/> buff;
            for (;;) {
                auto rc = pread(cgroup_procs_fd, buff.data(), buff.size(), 0);
                if (rc == 0) {
                    break; // no more pids
                }
                auto str = StringView{buff.data(), static_cast<size_t>(rc)};
                auto pos = str.find('\n');
                if (pos != StringView::npos) {
                    str = str.substring(0, pos);
                }
                auto written = write(others_cgroup_procs_fd, str.data(), str.size());
                if (written == -1) {
                    if (errno == ESRCH) {
                        continue; // process died before we changed its cgroup
                    }
                    do_die_with_error(error_fd, "write()");
                }
                if (static_cast<size_t>(written) != str.size()) {
                    do_die_with_msg(error_fd, "partial write()");
                }
            }
            // Release the lock here (not in execveat()) to minimize the critical section
            if (close(cgroup_procs_fd)) {
                do_die_with_error(error_fd, "close()");
            }

            // Enable needed controllers in the new cgroup (needs to be done after moving out
            // processes from the cgroup)
            cgroup_path.resize(cgroup_path.size() - std::strlen("others/cgroup.procs"));
            cgroup_path += "cgroup.subtree_control"; // won't throw
            write_file(error_fd, cgroup_path.c_str(), "+pids +memory +cpu");
            // Execute the supervisor
            char argv0[] = "";
            auto sock_as_str = to_string(sock_fd);
            char* const argv[] = {argv0, sock_as_str.data(), nullptr};
            char* const env[] = {nullptr};
            syscalls::execveat(supervisor_exe_fd, "", argv, env, AT_EMPTY_PATH);
            do_die_with_error(error_fd, "execveat()");
        }
    }

    // Execute the supervisor using systemd-run
    char str_systemd_run[] = "/usr/bin/systemd-run";
    char str_arg_user[] = "--user";
    char str_arg_scope[] = "--scope"; // preserve the execution environment
    char str_arg_property_delegate[] = "--property=Delegate=yes";
    char str_arg_collect[] = "--collect"; // prevent unit from prevailing after failure
    char str_arg_quiet[] = "--quiet";
    // systemd-run will inherit the file descriptor, so we can use "self" instead of getpid()
    auto executable_path = noexcept_concat("/proc/self/fd/", supervisor_exe_fd);
    auto sock_as_str = to_string(sock_fd);
    char* const argv[] = {
        str_systemd_run,
        str_arg_user,
        str_arg_scope,
        str_arg_property_delegate,
        str_arg_collect,
        str_arg_quiet,
        executable_path.data(),
        sock_as_str.data(),
        nullptr
    };
    char* xdg_runtime_dir = nullptr;
    for (auto env = environ; *env; ++env) {
        if (has_prefix(*env, "XDG_RUNTIME_DIR=")) {
            xdg_runtime_dir = *env;
            break;
        }
    }
    char* const env[] = {xdg_runtime_dir, nullptr};
    syscalls::execveat(AT_FDCWD, str_systemd_run, argv, env, 0);
    do_die_with_error(error_fd, "execveat()");
}

SupervisorConnection spawn_supervisor() {
    int supervisor_error_fd = memfd_create("sandbox supervisor errors", MFD_CLOEXEC);
    if (supervisor_error_fd == -1) {
        THROW("memfd_create()", errmsg());
    }

    // Create socket file descriptors with CLOEXEC flag so that fork() + execve() in the other
    // threads would not inherit these file descriptors
    int sock_fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sock_fds)) {
        int errnum = errno;
        (void)close(supervisor_error_fd);
        THROW("socketpair()", errmsg(errnum));
    }
    int sock_fd = sock_fds[0];
    int supervisor_sock_fd = sock_fds[1];

    // Sadly, we cannot use clone3() with .flags == CLONE_PIDFD because it causes malloc() to hang
    // indefinitely occasionally in the child process if the parent process (the current process) is
    // multi-threaded.
    auto pid = fork();
    if (pid == -1) {
        int errnum = errno;
        (void)close(supervisor_sock_fd);
        (void)close(sock_fd);
        (void)close(supervisor_error_fd);
        THROW("fork()", errmsg(errnum));
    }
    if (pid == 0) {
        // sock_fd and pipe_opt->readable will be closed by exec()
        execute_supervisor(supervisor_error_fd, supervisor_sock_fd);
    }

    // We don't have to synchronize with the child process, because pidfd_open() works on non-reaped
    // (even zombie) processes.
    int supervisor_pidfd = syscalls::pidfd_open(pid, 0);
    if (supervisor_pidfd < 0) {
        int errnum = errno;
        (void)close(supervisor_sock_fd);
        (void)close(sock_fd);
        (void)close(supervisor_error_fd);
        THROW("pidfd_open()", errmsg(errnum));
    }

    // Close the supervisor's end of the socket
    if (close(supervisor_sock_fd)) {
        int errnum = errno;
        (void)close(supervisor_pidfd);
        (void)close(sock_fd);
        (void)close(supervisor_error_fd);
        THROW("close()", errmsg(errnum));
    }

    return SupervisorConnection{sock_fd, supervisor_pidfd, supervisor_error_fd};
}

SupervisorConnection::KillRequestHandle::~KillRequestHandle() noexcept(false) {
    // We cannot throw during the stack unwinding
    bool can_throw = uncaught_exceptions_in_constructor == std::uncaught_exceptions();
    if (kill_fd != -1 && close(kill_fd) && can_throw) {
        THROW("close()", errmsg());
    }
}

void SupervisorConnection::KillRequestHandle::kill() {
    if (kill_fd != -1) {
        if (write_as_bytes(kill_fd, uint64_t{1})) {
            THROW("write()", errmsg());
        }
        if (close(std::exchange(kill_fd, -1))) {
            THROW("close()", errmsg());
        }
    }
}

SupervisorConnection::RequestHandle::RequestHandle(int result_fd, int kill_fd) noexcept
: result_fd{result_fd}
, kill_fd{kill_fd} {}

void SupervisorConnection::RequestHandle::do_cancel(bool can_throw) {
    if (is_cancelled()) {
        return;
    }
    if (close(std::exchange(result_fd, -1)) && can_throw) {
        THROW("close()", errmsg());
    }
}

SupervisorConnection::KillRequestHandle
SupervisorConnection::RequestHandle::get_kill_handle() noexcept {
    return KillRequestHandle{std::exchange(kill_fd, -1)};
}

SupervisorConnection::RequestHandle::~RequestHandle() noexcept(false) {
    // We cannot throw during the stack unwinding
    bool can_throw = uncaught_exceptions_in_constructor == std::uncaught_exceptions();
    do_cancel(can_throw);
    if (kill_fd != -1 && close(kill_fd) && can_throw) {
        THROW("close()", errmsg());
    }
}

SupervisorConnection::RequestHandle SupervisorConnection::do_send_request(
    std::variant<int, std::string_view> executable,
    Slice<const std::string_view> argv,
    const RequestOptions& options
) {
    if (supervisor_is_dead_and_waited()) {
        THROW("sandbox supervisor is already dead");
    }

    auto handle_send_error = [this](const char* msg) {
        int errnum = errno;
        kill_and_wait_supervisor_and_receive_errors(); // throws if there is an error
        THROW(msg, errmsg(errnum));
    };

    auto result_pipe = pipe2(O_CLOEXEC);
    if (!result_pipe) {
        THROW("pipe2()", errmsg());
    }

    auto kill_fd = FileDescriptor{eventfd(0, EFD_CLOEXEC)};
    if (!kill_fd.is_open()) {
        THROW("eventfd()", errmsg());
    }

    auto serialized_request =
        client::request::serialize(result_pipe->writable, kill_fd, executable, argv, options);
    auto rc = send_fds<serialized_request.fds.max_size()>(
        sock_fd,
        serialized_request.header.data(),
        serialized_request.header.size(),
        MSG_NOSIGNAL,
        serialized_request.fds.data(),
        serialized_request.fds.size()
    );
    if (rc < 0) {
        handle_send_error("sendmsg()");
    }
    // Send the remaining bytes if some were not sent
    if (send_exact(
            sock_fd,
            serialized_request.header.data() + rc,
            serialized_request.header.size() - rc,
            MSG_NOSIGNAL
        ))
    {
        handle_send_error("send()");
    }

    if (send_exact(
            sock_fd, serialized_request.body.get(), serialized_request.body_len, MSG_NOSIGNAL
        ))
    {
        handle_send_error("send()");
    }

    if (result_pipe->writable.close()) {
        THROW("close()", errmsg());
    }

    return RequestHandle{result_pipe->readable.release(), kill_fd.release()};
}

SupervisorConnection::RequestHandle SupervisorConnection::send_request(
    int executable_fd, Slice<const std::string_view> argv, const RequestOptions& options
) {
    return do_send_request(executable_fd, argv, options);
}

SupervisorConnection::RequestHandle SupervisorConnection::send_request(
    std::string_view executable_path,
    Slice<const std::string_view> argv,
    const RequestOptions& options
) {
    if (executable_path.empty()) {
        THROW("executable path cannot be empty");
    }
    return do_send_request(executable_path, argv, options);
}

SupervisorConnection::RequestHandle SupervisorConnection::send_request(
    Slice<const std::string_view> argv, const RequestOptions& options
) {
    if (argv.is_empty()) {
        THROW("argv cannot be empty");
    }
    return send_request(argv[0], argv, options);
}

Result SupervisorConnection::await_result(RequestHandle&& request_handle) {
    if (sock_fd == -1) {
        THROW("unable to read more requests");
    }
    if (request_handle.is_cancelled()) {
        THROW("unable to await request that is cancelled");
    }

    auto handle_read_error = [this] {
        int recv_errnum = errno;
        (void)close(sock_fd); // we already have an error from read_bytes_as()
        sock_fd = -1;
        // Receive errors if there are any
        auto si = kill_and_wait_supervisor_and_receive_errors(); // throws if there is an error
        if (is_one_of(recv_errnum, ECONNRESET, EPIPE)) {
            // Connection broke unexpectedly without apparent error from the supervisor.
            // kill_and_wait_supervisor_and_receive_errors() did not recognised an unexpected death,
            // but we  know that the supervisor died unexpectedly.
            THROW("sandbox supervisor died unexpectedly: ", si.description());
        }
        THROW("read()", errmsg(recv_errnum));
    };

    namespace response = communication::client_supervisor::response;
    response::error_len_t error_len;
    if (read_bytes_as(request_handle.result_fd, error_len)) {
        handle_read_error();
    }
    if (error_len == 0) {
        response::si::code_t tracee_si_code;
        response::si::status_t tracee_si_status;
        response::time::sec_t tracee_runtime_sec;
        response::time::nsec_t tracee_runtime_nsec;
        response::cgroup::usec_t tracee_cgroup_cpu_user_time;
        response::cgroup::usec_t tracee_cgroup_cpu_system_time;
        response::cgroup::peak_memory_in_bytes_t tracee_cgroup_peak_memory_in_bytes;
        if (read_bytes_as(
                request_handle.result_fd,
                tracee_si_code,
                tracee_si_status,
                tracee_runtime_sec,
                tracee_runtime_nsec,
                tracee_cgroup_cpu_user_time,
                tracee_cgroup_cpu_system_time,
                tracee_cgroup_peak_memory_in_bytes
            ))
        {
            handle_read_error();
        }
        static_assert(std::is_unsigned_v<decltype(tracee_runtime_sec)>, "need it to be >= 0");
        static_assert(std::is_unsigned_v<decltype(tracee_runtime_nsec)>, "need it to be >= 0");
        if (tracee_runtime_nsec >= 1'000'000'000) {
            THROW("BUG: invalid tracee_runtime_nsec: ", tracee_runtime_nsec);
        }
        return result::Ok{
            .si =
                {
                    .code = tracee_si_code,
                    .status = tracee_si_status,
                },
            .runtime = std::chrono::seconds{tracee_runtime_sec} +
                std::chrono::nanoseconds{tracee_runtime_nsec},
            .cgroup =
                {
                    .cpu_time =
                        {
                            .user = std::chrono::microseconds{tracee_cgroup_cpu_user_time},
                            .system = std::chrono::microseconds{tracee_cgroup_cpu_system_time},
                        },
                    .peak_memory_in_bytes = tracee_cgroup_peak_memory_in_bytes,
                },
        };
    }

    std::string error(error_len, '\0');
    if (read_exact(request_handle.result_fd, error.data(), error_len)) {
        handle_read_error();
    }
    return result::Error{
        .description = std::move(error),
    };
}

void SupervisorConnection::kill_and_wait_supervisor() noexcept {
    (void)syscalls::pidfd_send_signal(supervisor_pidfd, SIGKILL, nullptr, 0);
    // Try to wait the supervisor process, otherwise it will become zombie until this process dies
    (void)syscalls::waitid(P_PIDFD, supervisor_pidfd, nullptr, __WALL | WEXITED, nullptr);
    (void)close(supervisor_pidfd);
    (void)close(supervisor_error_fd);
    mark_supervisor_is_dead_and_waited();
}

Si SupervisorConnection::kill_and_wait_supervisor_and_receive_errors() {
    optional<int> pidfd_send_signal_errnum = std::nullopt;
    optional<int> waitid_errnum = std::nullopt;
    siginfo_t si; // No need to zero out si_pid field (see man 2 waitid)

    // Note that sending signal to the dead but not waited process is not an error.
    // Kill with SIGKILL to kill the supervisor even if it is stopped.
    if (syscalls::pidfd_send_signal(supervisor_pidfd, SIGKILL, nullptr, 0)) {
        pidfd_send_signal_errnum = errno;
    }
    // Try to wait the supervisor process, otherwise it will become zombie until this process dies
    if (syscalls::waitid(P_PIDFD, supervisor_pidfd, &si, __WALL | WEXITED, nullptr)) {
        waitid_errnum = errno;
    }

    optional<int> close_errnum = std::nullopt;
    if (close(supervisor_pidfd)) {
        close_errnum = errno;
    }

    mark_supervisor_is_dead_and_waited(
    ); // this has to be done before potentially throwing operations

    // Check for errors
    if (pidfd_send_signal_errnum) {
        (void)close(supervisor_error_fd);
        THROW("pidfd_send_signal()", errmsg(*pidfd_send_signal_errnum));
    }
    if (waitid_errnum) {
        (void)close(supervisor_error_fd);
        THROW("waitid()", errmsg(*waitid_errnum));
    }
    if (close_errnum) {
        (void)close(supervisor_error_fd);
        THROW("close()", errmsg(*close_errnum));
    }

    // Receive errors. The supervisor will not write more since it is dead by now.
    off64_t msg_len = lseek64(supervisor_error_fd, 0, SEEK_CUR);
    if (msg_len == -1) {
        int errnum = errno;
        (void)close(supervisor_error_fd);
        THROW("lseek()", errmsg(errnum));
    }
    if (msg_len > 0) {
        std::string msg;
        try {
            msg.resize(msg_len, '\0');
        } catch (...) {
            (void)close(supervisor_error_fd);
            throw;
        }
        auto rc = pread_all(supervisor_error_fd, 0, msg.data(), msg.size());
        if (errno) {
            int errnum = errno;
            (void)close(supervisor_error_fd);
            THROW("pread()", errmsg(errnum));
        }
        if (rc != msg.size()) {
            (void)close(supervisor_error_fd);
            msg.resize(rc);
            THROW("pread() - expected ", msg.size(), " bytes, but read just ", rc, " bytes: ", msg);
        }
        (void)close(supervisor_error_fd);
        THROW("sandbox: ", msg);
    }
    // Supervisor died without an error message
    if (close(supervisor_error_fd)) {
        THROW("close()", errmsg());
    }
    if (si.si_code == CLD_EXITED && si.si_status == 0) {
        return Si{.code = CLD_EXITED, .status = 0}; // supervisor saw the closed socket and exited
    }
    // Premature death for an unexpected reason
    if (si.si_code != CLD_KILLED || si.si_status != SIGKILL) {
        THROW("sandbox supervisor died unexpectedly: ", Si{si.si_code, si.si_status}.description());
    }
    return Si{.code = CLD_KILLED, .status = SIGKILL};
}

SupervisorConnection::~SupervisorConnection() noexcept(false) {
    // We cannot throw during the stack unwinding
    bool can_throw = uncaught_exceptions_in_constructor == std::uncaught_exceptions();
    // Kill the supervisor no matter if we are during the stack unwinding or we destruct normally.
    // It is faster than signaling it to exit and we don't lose any errors reported by the
    // supervisor so far.
    if (can_throw) {
        if (!supervisor_is_dead_and_waited()) {
            try {
                kill_and_wait_supervisor_and_receive_errors(); // may throw
            } catch (...) {
                if (sock_fd != -1) {
                    (void)close(sock_fd);
                }
                throw;
            }
        }
        if (sock_fd != -1 && close(sock_fd)) {
            THROW("close()", errmsg());
        }
    } else {
        if (!supervisor_is_dead_and_waited()) {
            kill_and_wait_supervisor();
        }
        if (sock_fd != -1) {
            (void)close(sock_fd);
        }
    }
}

} // namespace sandbox
