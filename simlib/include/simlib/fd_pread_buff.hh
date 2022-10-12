#pragma once

#include <array>
#include <cstdio>
#include <optional>
#include <sys/types.h>
#include <unistd.h>

class FdPreadBuff {
    int fd;
    std::array<unsigned char, 4096> buff{};
    size_t pos = 0;
    size_t len = 0;
    off64_t buff_end_offset;

public:
    explicit FdPreadBuff(int fd, off64_t beg_offset = 0) noexcept
    : fd{fd}
    , buff_end_offset{beg_offset} {}

    // On success returns read character or EOF on success, otherwise
    // std::nullopt is returned and errno is set by read()
    std::optional<int> get_char() noexcept {
        if (pos != len) {
            return buff[pos++];
        }
        ssize_t res = pread64(fd, buff.data(), buff.size(), buff_end_offset);
        if (res < 0) {
            return std::nullopt;
        }
        if (res == 0) {
            return EOF;
        }
        buff_end_offset += res;
        len = res;
        pos = 1;
        return buff[0];
    }

    [[nodiscard]] off64_t curr_offset() const noexcept { return buff_end_offset - (len - pos); }
};
