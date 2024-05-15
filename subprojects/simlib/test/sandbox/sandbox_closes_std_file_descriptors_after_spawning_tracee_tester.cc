#include <cstring>
#include <poll.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }

    if (std::strlen(argv[1]) != 2) {
        return 2;
    }
    auto to_fd = [](char c) {
        switch (c) {
        case 'i': return STDIN_FILENO;
        case 'o': return STDOUT_FILENO;
        case 'e': return STDERR_FILENO;
        default: _exit(3);
        }
    };

    int in_fd = to_fd(argv[1][0]);
    int out_fd = to_fd(argv[1][1]);

    pollfd pfd = {
        .fd = out_fd,
        .events = 0,
        .revents = 0,
    };
    if (poll(&pfd, 1, 0) != 0) {
        return 4;
    }
    if (close(in_fd)) {
        return 5;
    }
    // Wait for supervisor and pid1 to close the file descriptors
    if (poll(&pfd, 1, 1000) != 1) {
        return 6;
    }
    if (pfd.revents != POLLHUP) {
        return 7;
    }
}
