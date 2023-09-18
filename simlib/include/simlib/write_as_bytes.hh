#pragma once

#include <cstddef>
#include <cstring>
#include <simlib/write_exact.hh>
#include <type_traits>

template <class... Ts, std::enable_if_t<(std::is_trivial_v<Ts> && ...), int> = 0>
int write_as_bytes(int fd, const Ts&... vals) noexcept {
    if constexpr ((sizeof(vals) + ... + 0) > 0) {
        char data[(sizeof(vals) + ...)];
        size_t offset = 0;
        ((std::memcpy(data + offset, &vals, sizeof(vals)), offset += sizeof(vals)), ...);
        if (write_exact(fd, data, sizeof(data))) {
            return -1;
        }
    }
    return 0;
}
