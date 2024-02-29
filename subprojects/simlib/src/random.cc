#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/random.hh>
#include <sys/random.h>

void fill_randomly(void* dest, size_t bytes) {
    while (bytes > 0) {
        ssize_t len = getrandom(dest, bytes, 0);
        if (len >= 0) {
            bytes -= len;
        } else if (errno != EINTR) {
            THROW("getrandom()", errmsg());
        }
    }
}

ssize_t read_from_dev_urandom_nothrow(void* dest, size_t bytes) noexcept {
    FileDescriptor fd("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        return -1;
    }

    size_t len = read_all(fd, dest, bytes);
    int errnum = errno;

    if (bytes == len) {
        return bytes;
    }

    errno = errnum;
    return -1;
}

void read_from_dev_urandom(void* dest, size_t bytes) {
    FileDescriptor fd("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        THROW("Failed to open /dev/urandom", errmsg());
    }

    size_t len = read_all(fd, dest, bytes);
    int errnum = errno;

    if (len != bytes) {
        THROW("Failed to read from /dev/urandom", errmsg(errnum));
    }
}
