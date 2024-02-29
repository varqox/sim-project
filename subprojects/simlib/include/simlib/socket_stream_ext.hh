#pragma once

#include <cstring>
#include <sys/socket.h>
#include <type_traits>

// Same as recv(2) but errno != EINTR and if stream ends before reading @p len bytes, returns -1 and
// sets errno to EPIPE. On success returns 0.
int send_exact(int sock_fd, const void* buf, size_t len, int flags) noexcept;

// Same as recv(2) but errno != EINTR and if stream ends before reading @p len bytes, returns -1 and
// sets errno to EPIPE. On success returns 0.
int recv_exact(int sock_fd, void* buf, size_t len, int flags) noexcept;

template <class... Ts, std::enable_if_t<(std::is_trivial_v<Ts> && ...), int> = 0>
int send_as_bytes(int sock_fd, int flags, const Ts&... vals) noexcept {
    char data[(sizeof(vals) + ...)];
    size_t offset = 0;
    ((std::memcpy(data + offset, &vals, sizeof(vals)), offset += sizeof(vals)), ...);
    if (send_exact(sock_fd, data, sizeof(data), flags)) {
        return -1;
    }
    return 0;
}

template <class... Ts, std::enable_if_t<(std::is_trivial_v<Ts> && ...), int> = 0>
int recv_bytes_as(int sock_fd, int flags, Ts&... vals) noexcept {
    char data[(sizeof(vals) + ...)];
    if (recv_exact(sock_fd, data, sizeof(data), flags)) {
        return -1;
    }
    size_t offset = 0;
    ((std::memcpy(&vals, data + offset, sizeof(vals)), offset += sizeof(vals)), ...);
    return 0;
}
