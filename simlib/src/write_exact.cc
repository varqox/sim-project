#include <cerrno>
#include <simlib/macros/likely.hh>
#include <simlib/write_exact.hh>
#include <unistd.h>

int write_exact(int sock_fd, const void* buf, size_t len) noexcept {
    size_t sent = 0;
    while (sent < len) {
        auto rc = write(sock_fd, static_cast<const char*>(buf) + sent, len - sent);
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
