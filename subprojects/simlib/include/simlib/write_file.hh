#pragma once

#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_path.hh>
#include <simlib/file_perms.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_view.hh>

inline void write_file(FilePath path, StringView data, mode_t mode = S_0644) {
    auto fd = FileDescriptor{path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode};
    if (fd < 0) {
        THROW("open()", errmsg());
    }
    size_t written = 0;
    while (written < data.size()) {
        auto res = write(fd, data.data() + written, data.size() - written);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
            THROW("write()", errmsg());
        }
        written += res;
    }
}
