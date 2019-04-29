#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/random.h"

#include <sys/syscall.h>

RandomDevice random_generator;

void fillRandomly(void* dest, size_t bytes) {
	while (bytes > 0) {
		ssize_t len = syscall(SYS_getrandom, dest, bytes, 0);
		if (len >= 0)
			bytes -= len;
		else if (errno != EINTR)
			THROW("getrandom()", errmsg());
	}
}

ssize_t readFromDevUrandom_nothrow(void* dest, size_t bytes) noexcept {
	FileDescriptor fd("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return -1;

	size_t len = readAll(fd, dest, bytes);
	int errnum = errno;

	if (bytes == len)
		return bytes;

	errno = errnum;
	return -1;
}

void readFromDevUrandom(void* dest, size_t bytes) {
	FileDescriptor fd("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		THROW("Failed to open /dev/urandom", errmsg());

	size_t len = readAll(fd, dest, bytes);
	int errnum = errno;

	if (len != bytes)
		THROW("Failed to read from /dev/urandom", errmsg(errnum));
}
