#include "communication/client_supervisor.hh"
#include "do_die_with_error.hh"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <optional>
#include <simlib/array_buff.hh>
#include <simlib/array_vec.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/socket_send_fds.hh>
#include <simlib/socket_stream_ext.hh>
#include <simlib/syscalls.hh>
#include <simlib/to_string.hh>
#include <simlib/utilities.hh>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

extern "C" {
extern const unsigned char sandbox_supervisor_dump[];
extern const unsigned int sandbox_supervisor_dump_len;
} // extern "C"

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

    int supervisor_exe_fd = memfd_create("supervisor executable", MFD_CLOEXEC);
    if (supervisor_exe_fd == -1) {
        do_die_with_error(error_fd, "memfd_create()");
    }
    if (write_all(supervisor_exe_fd, sandbox_supervisor_dump, sandbox_supervisor_dump_len) !=
        sandbox_supervisor_dump_len)
    {
        do_die_with_error(error_fd, "write()");
    }

    // Execute the supervisor
    auto sock_as_str = to_string(sock_fd);
    char argv0[] = "sandbox_supervisor";
    char* const argv[] = {argv0, sock_as_str.data(), nullptr};
    char* const env[] = {nullptr};
    syscalls::execveat(supervisor_exe_fd, "", argv, env, AT_EMPTY_PATH);
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

    int supervisor_pidfd = 0;
    clone_args cl_args = {};
    cl_args.flags = CLONE_PIDFD;
    cl_args.pidfd = reinterpret_cast<uint64_t>(&supervisor_pidfd);
    cl_args.exit_signal = 0; // we don't need SIGCHLD
    auto pid = syscalls::clone3(&cl_args);
    if (pid == -1) {
        int errnum = errno;
        (void)close(supervisor_sock_fd);
        (void)close(sock_fd);
        (void)close(supervisor_error_fd);
        THROW("clone3()", errmsg(errnum));
    }
    if (pid == 0) {
        // sock_fd will be closed by exec()
        execute_supervisor(supervisor_error_fd, supervisor_sock_fd);
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

void SupervisorConnection::send_request(
    int executable_fd, Slice<std::string_view> argv, const RequestOptions& options
) {
    if (supervisor_is_dead_and_waited()) {
        THROW("sandbox supervisor is already dead");
    }

    auto handle_send_error = [this](const char* msg) {
        int errnum = errno;
        kill_and_wait_supervisor_and_receive_errors(); // throws if there is an error
        THROW(msg, errmsg(errnum));
    };

    namespace request = communication::client_supervisor::request;

    ArrayVec<int, 4> fds{executable_fd};
    request::mask_t mask = 0;
    if (options.stdin_fd) {
        fds.push(*options.stdin_fd);
        mask |= request::mask::sending_stdin_fd;
    }
    if (options.stdout_fd) {
        fds.push(*options.stdout_fd);
        mask |= request::mask::sending_stdout_fd;
    }
    if (options.stderr_fd) {
        fds.push(*options.stderr_fd);
        mask |= request::mask::sending_stderr_fd;
    }

    request::serialized_argv_len_t serialized_argv_len = 0;
    for (const auto& arg : argv) {
        serialized_argv_len += arg.size() + 1;
    }
    request::serialized_env_len_t serialized_env_len = 0;
    for (const auto& str : options.env) {
        serialized_env_len += str.size() + 1;
    }

    auto header_buff = array_buff_from(mask, serialized_argv_len, serialized_env_len);
    auto rc = send_fds<fds.max_size()>(
        sock_fd, header_buff.data(), header_buff.size(), MSG_NOSIGNAL, fds.data(), fds.size()
    );
    if (rc < 0) {
        handle_send_error("sendmsg()");
    }
    // Send the remaining bytes if some were not sent
    if (send_exact(sock_fd, header_buff.data() + rc, header_buff.size() - rc, MSG_NOSIGNAL)) {
        handle_send_error("send()");
    }

    std::vector<uint8_t> buff;
    buff.reserve(serialized_argv_len + serialized_env_len);
    for (const auto& arg : argv) {
        buff.insert(buff.end(), arg.begin(), arg.end());
        buff.emplace_back('\0');
    }
    assert(buff.size() == serialized_argv_len);
    for (const auto& str : options.env) {
        buff.insert(buff.end(), str.begin(), str.end());
        buff.emplace_back('\0');
    }
    assert(buff.size() == serialized_argv_len + serialized_env_len);

    if (send_exact(sock_fd, buff.data(), buff.size(), MSG_NOSIGNAL)) {
        handle_send_error("send()");
    }
}

void SupervisorConnection::send_request(
    Slice<std::string_view> argv, const RequestOptions& options
) {
    if (argv.is_empty()) {
        THROW("argv cannot be empty");
    }
    FileDescriptor exe_fd{open(std::string(argv[0]).c_str(), O_RDONLY | O_CLOEXEC)};
    if (!exe_fd.is_open()) {
        THROW("open(\"", argv[0], "\")", errmsg());
    }
    return send_request(exe_fd, argv, options);
}

Result SupervisorConnection::await_result() {
    if (sock_fd == -1) {
        THROW("unable to read more requests");
    }

    auto handle_recv_error = [this] {
        int recv_errnum = errno;
        (void)close(sock_fd); // we already have an error from recv_bytes_as()
        sock_fd = -1;
        // Receive errors if there are any
        auto si = kill_and_wait_supervisor_and_receive_errors(); // throws if there is an error
        if (is_one_of(recv_errnum, ECONNRESET, EPIPE)) {
            // Connection broke unexpectedly without apparent error from the supervisor.
            // kill_and_wait_supervisor_and_receive_errors() did not recognised an unexpected death,
            // but we  know that the supervisor died unexpectedly.
            THROW("sandbox supervisor died unexpectedly: ", si.description());
        }
        THROW("recv()", errmsg(recv_errnum));
    };

    namespace response = communication::client_supervisor::response;
    response::error_len_t error_len;
    if (recv_bytes_as(sock_fd, 0, error_len)) {
        handle_recv_error();
    }
    if (error_len == 0) {
        response::si::code_t tracee_si_code;
        response::si::status_t tracee_si_status;
        response::time::sec_t tracee_runtime_sec;
        response::time::nsec_t tracee_runtime_nsec;
        if (recv_bytes_as(
                sock_fd,
                0,
                tracee_si_code,
                tracee_si_status,
                tracee_runtime_sec,
                tracee_runtime_nsec
            ))
        {
            handle_recv_error();
        }
        if (tracee_runtime_sec < 0) {
            THROW("BUG: invalid tracee_runtime_sec: ", tracee_runtime_sec);
        }
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
        };
    }

    std::string error(error_len, '\0');
    if (recv_exact(sock_fd, error.data(), error_len, 0)) {
        handle_recv_error();
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
    std::optional<int> pidfd_send_signal_errnum = std::nullopt;
    std::optional<int> waitid_errnum = std::nullopt;
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

    std::optional<int> close_errnum = std::nullopt;
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
