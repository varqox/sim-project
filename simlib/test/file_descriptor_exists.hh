#pragma once

#include <fcntl.h>

inline bool file_descriptor_exists(int fd) noexcept {
	return (fcntl(fd, F_GETFD) != -1);
}
