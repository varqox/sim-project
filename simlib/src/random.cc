#include "../include/filesystem.h"
#include "../include/logger.h"

#include <unistd.h>

ssize_t readRandomBytes_nothrow(void* dest, size_t bytes) noexcept {
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		return -1;

	size_t len = read(fd, dest, bytes);
	sclose(fd);

	return (bytes == len ? bytes : -1);
}

void readRandomBytes(void* dest, size_t bytes) noexcept(false) {
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		throw std::runtime_error(concat("Failed to open /dev/urandom",
			error(errno)));

	size_t len = read(fd, dest, bytes);
	sclose(fd);

	if (len != bytes)
		throw std::runtime_error(concat("Failed to read from /dev/urandom",
			error(errno)));
}
