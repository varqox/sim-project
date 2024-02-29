#pragma once

#include <cstring>
#include <simlib/read_exact.hh>
#include <type_traits>

template <class... Ts, std::enable_if_t<(std::is_trivial_v<Ts> && ...), int> = 0>
int read_bytes_as(int fd, Ts&... vars) noexcept {
    if constexpr ((sizeof(vars) + ... + 0) > 0) {
        char data[(sizeof(vars) + ...)];
        if (read_exact(fd, data, sizeof(data))) {
            return -1;
        }
        size_t offset = 0;
        ((std::memcpy(&vars, data + offset, sizeof(vars)), offset += sizeof(vars)), ...);
    }
    return 0;
}
