#pragma once

#include <array>
#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_path.hh>
#include <simlib/macros/throw.hh>
#include <string>

inline std::string read_file(int fd) {
    std::string res;
    std::array<char, 1 << 14> buff;
    for (;;) {
        auto len = read(fd, buff.data(), buff.size());
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            THROW("read()", errmsg());
        }
        if (len == 0) {
            return res;
        }
        res.append(buff.data(), buff.data() + len);
    }
}

inline std::string read_file(FilePath path) {
    auto fd = FileDescriptor{path, O_RDONLY | O_CLOEXEC};
    if (fd < 0) {
        THROW("open()", errmsg());
    }
    return read_file(fd);
}
