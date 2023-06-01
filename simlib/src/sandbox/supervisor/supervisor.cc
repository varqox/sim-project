#include "../communication/client_supervisor.hh"
#include "../do_die_with_error.hh"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <simlib/socket_stream_ext.hh>
#include <simlib/string_transform.hh>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using std::string;

namespace sandbox::supervisor {

struct CmdArgs {
    int sock_fd;
};

[[nodiscard]] CmdArgs parse_args(int argc, char** argv) noexcept {
    if (argc != 2) {
        do_die_with_msg(
            STDERR_FILENO,
            "Usage: ",
            (argv[0] == nullptr || argv[0][0] == '\0' ? "sandbox-supervisor" : argv[0]),
            " <unix socket file descriptor number>"
        );
    }

    auto sock_fd_opt = str2num<int>(argv[1]);
    if (not sock_fd_opt) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid file descriptor number as argument");
    }

    return {
        .sock_fd = *sock_fd_opt,
    };
}

void validate_sock_fd(int sock_fd) noexcept {
    auto get_sock_opt = [sock_fd](auto sock_opt) noexcept {
        int optval;
        socklen_t optlen = sizeof(optval);
        if (getsockopt(sock_fd, SOL_SOCKET, sock_opt, &optval, &optlen)) {
            do_die_with_msg(
                STDERR_FILENO, "supervisor: invalid file descriptor (getsockopt()", errmsg(), ")"
            );
        }
        assert(optlen == sizeof(optval));
        return optval;
    };

    if (get_sock_opt(SO_DOMAIN) != AF_UNIX) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid socket domain, expected AF_UNIX");
    }

    // We want unlimited message size and send()/recv() errors if the second end is closed
    if (get_sock_opt(SO_TYPE) != SOCK_STREAM) {
        do_die_with_msg(STDERR_FILENO, "supervisor: invalid socket type, expected SOCK_STREAM");
    }
}

constexpr int ERRORS_FD = STDERR_FILENO + 1;
constexpr int SOCK_FD = STDERR_FILENO + 2;

void initialize_sock_fd_and_error_fd(int sock_fd) noexcept {
    // There can be trouble if sock_fd is one of {STDIN,STDOUT,STDERR}_FILENO because we will reopen
    // them, so we make sure to use a different fd
    static_assert(
        SOCK_FD != STDERR_FILENO, "we cannot lose STDERR_FILENO in the following operations"
    );
    if (sock_fd == SOCK_FD) {
        // Set CLOEXEC flag so that we don't leak the file descriptor on exec()
        if (fcntl(SOCK_FD, F_SETFD, FD_CLOEXEC)) {
            do_die_with_error(STDERR_FILENO, "supervisor: fcntl()");
        }
    } else {
        if (dup3(sock_fd, SOCK_FD, O_CLOEXEC) == -1) {
            do_die_with_error(STDERR_FILENO, "supervisor: dup3()");
        }
    }

    static_assert(ERRORS_FD != SOCK_FD, "we cannot lose SOCK_FD in the following operations");
    static_assert(STDERR_FILENO != ERRORS_FD, "stderr will be reopen as /dev/null");
    if (dup3(STDERR_FILENO, ERRORS_FD, O_CLOEXEC) == -1) {
        do_die_with_error(STDERR_FILENO, "supervisor: dup3()");
    }
}

template <class... Args>
[[noreturn]] void die_with_error(Args&&... msg) noexcept {
    sandbox::do_die_with_error(ERRORS_FD, "supervisor: ", std::forward<Args>(msg)...);
}

void close_all_stray_file_descriptors() noexcept {
    static_assert(STDIN_FILENO == 0);
    static_assert(STDOUT_FILENO == 1);
    static_assert(STDERR_FILENO == 2);
    static_assert(ERRORS_FD == 3);
    static_assert(SOCK_FD == 4);
    if (close_range(SOCK_FD + 1, ~0U, 0)) {
        die_with_error("close_range()");
    }
}

void replace_stdin_stdout_stderr_with_dev_null() noexcept {
    int fd = open("/dev/null", O_RDONLY); // NOLINT(android-cloexec-open)
    if (fd == -1) {
        die_with_error("open(/dev/null)");
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        die_with_error("dup2()");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
    fd = open("/dev/null", O_WRONLY); // NOLINT(android-cloexec-open)
    if (fd == -1) {
        die_with_error("open(/dev/null)");
    }
    if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
        die_with_error("dup2()");
    }
    if (close(fd)) {
        die_with_error("close()");
    }
}

namespace request = communication::client_supervisor::request;

struct Request {
    string shell_command;
};

Request recv_request() noexcept {
    request::shell_command_len_t shell_command_len;
    if (recv_bytes_as(SOCK_FD, 0, shell_command_len)) {
        if (errno == EPIPE) {
            _exit(0); // No more requests
        }
        die_with_error("recv()");
    }

    std::string shell_command(shell_command_len, '\0');
    if (recv_exact(SOCK_FD, shell_command.data(), shell_command_len, 0)) {
        die_with_error("recv()");
    }
    return {
        .shell_command = std::move(shell_command),
    };
}

namespace response = communication::client_supervisor::response;

void send_response_ok(response::Ok res) noexcept {
    response::error_len_t error_len = 0;
    if (send_exact(SOCK_FD, &error_len, sizeof(error_len), 0)) {
        die_with_error("send()");
    }
    if (send_exact(SOCK_FD, &res, sizeof(res), 0)) {
        die_with_error("send()");
    }
}

void main(int argc, char** argv) noexcept {
    auto args = parse_args(argc, argv);
    validate_sock_fd(args.sock_fd);
    initialize_sock_fd_and_error_fd(args.sock_fd);
    close_all_stray_file_descriptors();
    replace_stdin_stdout_stderr_with_dev_null();

    for (;;) {
        auto request = recv_request();
        send_response_ok({.system_result = system(request.shell_command.c_str())});
    }
}

} // namespace sandbox::supervisor

int main(int argc, char** argv) noexcept {
    sandbox::supervisor::main(argc, argv);
    return 0;
}
