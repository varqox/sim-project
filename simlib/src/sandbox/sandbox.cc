#include "do_die_with_error.hh"

#include <cerrno>
#include <exception>
#include <fcntl.h>
#include <optional>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/macros/throw.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sandbox/si.hh>
#include <simlib/syscalls.hh>
#include <simlib/to_string.hh>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

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

// NOLINTNEXTLINE(readability-make-member-function-const)
void SupervisorConnection::kill_and_wait_supervisor() noexcept {
    (void)syscalls::pidfd_send_signal(supervisor_pidfd, SIGKILL, nullptr, 0);
    // Try to wait the supervisor process, otherwise it will become zombie until this process dies
    (void)syscalls::waitid(P_PIDFD, supervisor_pidfd, nullptr, __WALL | WEXITED, nullptr);
    (void)close(sock_fd);
    (void)close(supervisor_pidfd);
    (void)close(supervisor_error_fd);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void SupervisorConnection::kill_and_wait_supervisor_and_receive_errors() {
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
    if (close(sock_fd)) {
        close_errnum = errno;
    }
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
    // Premature death for an unexpected reason
    if (si.si_code != CLD_KILLED || si.si_status != SIGKILL) {
        THROW("sandbox supervisor died unexpectedly: ", Si{si.si_code, si.si_status}.description());
    }
}

SupervisorConnection::~SupervisorConnection() noexcept(false) {
    // We cannot throw during the stack unwinding
    bool can_throw = uncaught_exceptions_in_constructor == std::uncaught_exceptions();
    // Kill the supervisor no matter if we are during the stack unwinding or we destruct normally.
    // It is faster than signaling it to exit and we don't lose any errors reported by the
    // supervisor so far.
    if (can_throw) {
        kill_and_wait_supervisor_and_receive_errors();
    } else {
        kill_and_wait_supervisor();
    }
}

} // namespace sandbox
