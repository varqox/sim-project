#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <sys/types.h>
#include <unistd.h>

using std::array;
using std::string;

size_t read_all(int fd, void* buf, size_t count) noexcept {
    ssize_t k = 0;
    size_t pos = 0;
    auto* buff = static_cast<uint8_t*>(buf);
    while (pos < count) {
        k = read(fd, buff + pos, count - pos);
        if (k > 0) {
            pos += k;
        } else if (k == 0) {
            errno = 0; // No error
            return pos;

        } else if (errno != EINTR) {
            return pos; // Error
        }
    }

    errno = 0; // No error
    return count;
}

size_t pread_all(int fd, off64_t pos, void* buff, size_t count) noexcept {
    auto* dest = static_cast<uint8_t*>(buff);
    size_t bytes_read = 0;
    while (bytes_read < count) {
        ssize_t k = pread(fd, dest + bytes_read, count - bytes_read, pos + bytes_read);
        if (k < 0) {
            if (errno == EINTR) {
                continue;
            }
            return pos; // Error
        }
        if (k == 0) {
            break; // No more bytes to read
        }
        bytes_read += k;
    }

    errno = 0;
    return bytes_read;
}

size_t write_all(int fd, const void* buf, size_t count) noexcept {
    ssize_t k = 0;
    size_t pos = 0;
    const auto* buff = static_cast<const uint8_t*>(buf);
    errno = 0;
    while (pos < count) {
        k = write(fd, buff + pos, count - pos);
        if (k >= 0) {
            pos += k;
        } else if (errno != EINTR) {
            return pos; // Error
        }
    }

    errno = 0; // No error (need to set again because errno may equal to EINTR)
    return count;
}

string get_file_contents(int fd, size_t bytes) {
    string res;
    array<char, 65536> buff{};
    while (bytes > 0) {
        ssize_t len = read(fd, buff.data(), std::min(buff.size(), bytes));
        // Interrupted by signal
        if (len < 0 && errno == EINTR) {
            continue;
        }
        // Error
        if (len < 0) {
            THROW("read() failed", errmsg());
        }
        // EOF
        if (len == 0) {
            break;
        }

        bytes -= len;
        res.append(buff.data(), len);
    }

    return res;
}

string get_file_contents(int fd, off64_t beg, off64_t end) {
    array<char, 65536> buff{};
    off64_t size = lseek64(fd, 0, SEEK_END);
    if (beg < 0) {
        beg = std::max<off64_t>(size + beg, 0);
    }
    if (size == static_cast<off64_t>(-1)) {
        THROW("lseek64() failed", errmsg());
    }
    if (beg > size) {
        return "";
    }

    // Change end to the valid value
    if (size < end || end < 0) {
        end = size;
    }

    if (end < beg) {
        end = beg;
    }
    // Reposition to beg
    if (beg != lseek64(fd, beg, SEEK_SET)) {
        THROW("lseek64() failed", errmsg());
    }

    off64_t bytes_left = end - beg;
    string res;
    while (bytes_left > 0) {
        ssize_t len = read(fd, buff.data(), std::min<off64_t>(buff.size(), bytes_left));
        // Interrupted by signal
        if (len < 0 && errno == EINTR) {
            continue;
        }
        // Error
        if (len < 0) {
            THROW("read() failed", errmsg());
        }
        // EOF
        if (len == 0) {
            break;
        }

        bytes_left -= len;
        res.append(buff.data(), len);
    }

    return res;
}

string get_file_contents(FilePath file) {
    FileDescriptor fd;
    for (;;) {
        fd.open(file, O_RDONLY | O_CLOEXEC);
        if (fd.is_open() or errno != EINTR) {
            break;
        }
    }

    if (not fd.is_open()) {
        THROW("Failed to open file `", file, '`', errmsg());
    }

    return get_file_contents(fd);
}

string get_file_contents(FilePath file, off64_t beg, off64_t end) {
    FileDescriptor fd;
    for (;;) {
        fd.open(file, O_RDONLY | O_CLOEXEC);
        if (fd.is_open() or errno != EINTR) {
            break;
        }
    }

    if (not fd.is_open()) {
        THROW("Failed to open file `", file, '`', errmsg());
    }

    return get_file_contents(fd, beg, end);
}

void put_file_contents(FilePath file, const char* data, size_t len, mode_t mode) {
    FileDescriptor fd{file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode};
    if (fd == -1) {
        THROW("open() failed", errmsg());
    }

    if (len == size_t(-1)) {
        len = std::char_traits<char>::length(data);
    }

    if (write_all(fd, data, len) != len) {
        THROW("write() failed", errmsg());
    }
}
