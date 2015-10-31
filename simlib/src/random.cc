#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/ncg.h"

#include <unistd.h>

void fillRandomly(void* dest, size_t bytes) noexcept {
	if (bytes == 0)
		return;

	constexpr size_t len = sizeof(uint32_t);
	uint32_t* ptr = static_cast<uint32_t*>(dest);
	for (; bytes >= len; bytes -= len, ++ptr)
		*ptr = pull();

	// Fill last bytes
	if (bytes > 0) {
		union {
			uint32_t x = pull();
			uint8_t t[len];
		};
		uint8_t* ptr1 = reinterpret_cast<uint8_t*>(ptr);
		for (unsigned i = 0; i < bytes; ++i)
			ptr1[i] = t[i];
	}
}

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
