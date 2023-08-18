#include "../do_die_with_error.hh"

#include <simlib/string_transform.hh>
#include <sys/socket.h>
#include <unistd.h>

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

void main(int argc, char** argv) noexcept {
    auto args = parse_args(argc, argv);
    validate_sock_fd(args.sock_fd);

    char buff[1];
    (void)read(args.sock_fd, buff, sizeof(buff));
}

} // namespace sandbox::supervisor

int main(int argc, char** argv) noexcept {
    sandbox::supervisor::main(argc, argv);
    return 0;
}
