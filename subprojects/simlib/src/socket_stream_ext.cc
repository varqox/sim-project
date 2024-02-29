#include <cerrno>
#include <simlib/macros/likely.hh>
#include <simlib/socket_stream_ext.hh>
#include <unistd.h>

int send_exact(int sock_fd, const void* buf, size_t len, int flags) noexcept {
    size_t sent = 0;
    while (sent < len) {
        auto rc = send(sock_fd, static_cast<const char*>(buf) + sent, len - sent, flags);
        if (UNLIKELY(rc == 0)) {
            errno = EPIPE;
            return -1;
        }
        if (UNLIKELY(rc < 0)) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += rc;
    }
    return 0;
}

int recv_exact(int sock_fd, void* buf, size_t len, int flags) noexcept {
    size_t received = 0;
    while (received < len) {
        auto rc = recv(sock_fd, static_cast<char*>(buf) + received, len - received, flags);
        if (UNLIKELY(rc == 0)) {
            errno = EPIPE;
            return -1;
        }
        if (UNLIKELY(rc < 0)) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        received += rc;
    }
    return 0;
}
