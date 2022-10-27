#pragma once

#include <optional>
#include <simlib/file_descriptor.hh>
#include <unistd.h>

struct Pipe {
    FileDescriptor readable;
    FileDescriptor writable;
};

// Returns std::nullopt on error with errno set appropriately
inline std::optional<Pipe> pipe2(int flags) noexcept {
    int pfd[2];
    int rc = pipe2(pfd, flags);
    if (rc) {
        return std::nullopt;
    }
    return Pipe{
            .readable = FileDescriptor{pfd[0]},
            .writable = FileDescriptor{pfd[1]},
    };
}
