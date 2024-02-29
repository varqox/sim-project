#pragma once

#include <cstddef>

// Same as write(2) but errno != EINTR and if fd ends before writing @p len bytes, returns -1 and
// sets errno to EPIPE. On success returns 0.
int write_exact(int fd, const void* buf, size_t len) noexcept;
