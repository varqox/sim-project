#include <cerrno>
#include <simlib/macros/likely.hh>
#include <simlib/read_exact.hh>
#include <unistd.h>

int read_exact(int fd, void* buf, size_t len) noexcept {
    size_t received = 0;
    while (received < len) {
        auto rc = read(fd, static_cast<char*>(buf) + received, len - received);
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
