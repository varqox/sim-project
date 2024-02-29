#pragma once

#include <cstddef>

// Same as read(2) but errno != EINTR and if fd ends before reading @p len bytes, returns -1 and
// sets errno to EPIPE. On success returns 0.
int read_exact(int fd, void* buf, size_t len) noexcept;
