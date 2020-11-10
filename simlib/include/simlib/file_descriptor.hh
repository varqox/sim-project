#pragma once

#include "simlib/file_path.hh"
#include "simlib/file_perms.hh"

#include <unistd.h>

// Encapsulates file descriptor
class FileDescriptor {
    int fd_;

public:
    explicit FileDescriptor(int fd = -1) noexcept
    : fd_(fd) {}

    explicit FileDescriptor(FilePath filename, int flags, mode_t mode = S_0644) noexcept
    : fd_(::open(filename, flags, mode)) {}

    FileDescriptor(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& fd) noexcept
    : fd_(fd.release()) {}

    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor& operator=(FileDescriptor&& fd) noexcept {
        reset(fd.release());
        return *this;
    }

    FileDescriptor& operator=(int fd) noexcept {
        reset(fd);
        return *this;
    }

    [[nodiscard]] bool is_open() const noexcept { return (fd_ >= 0); }

    // To check for validity opened() should be used
    explicit operator bool() const noexcept = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator int() const noexcept { return fd_; }

    [[nodiscard]] int release() noexcept {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd) noexcept {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
        fd_ = fd;
    }

    void open(FilePath filename, int flags, int mode = S_0644) noexcept {
        fd_ = ::open(filename, flags, mode);
    }

    void reopen(FilePath filename, int flags, int mode = S_0644) noexcept {
        reset(::open(filename, flags, mode));
    }

    [[nodiscard]] int close() noexcept {
        if (fd_ < 0) {
            return 0;
        }

        int rc = ::close(fd_);
        fd_ = -1;
        return rc;
    }

    ~FileDescriptor() {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }
};
