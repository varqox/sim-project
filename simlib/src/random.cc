#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/random.h"

#include <unistd.h>

std::mt19937 random_generator(getRandomSeed());

void fillRandomly(void* dest, size_t bytes) noexcept {
	if (bytes == 0)
		return;

	constexpr size_t byte_length = sizeof(uint32_t);
	uint32_t* ptr = static_cast<uint32_t*>(dest);
	for (; bytes >= byte_length; bytes -= byte_length, ++ptr)
		*ptr = random_generator();

	// Fill last bytes
	if (bytes > 0) {
		union {
			uint32_t x = random_generator();
			uint8_t t[byte_length];
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

	size_t len = readAll(fd, dest, bytes);
	int errnum = errno;
	sclose(fd);

	if (bytes == len)
		return bytes;

	errno = errnum;
	return -1;
}

void readRandomBytes(void* dest, size_t bytes) noexcept(false) {
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		throw std::runtime_error(concat("Failed to open /dev/urandom",
			error(errno)));

	size_t len = readAll(fd, dest, bytes);
	int errnum = errno;
	sclose(fd);

	if (len != bytes)
		throw std::runtime_error(concat("Failed to read from /dev/urandom",
			error(errnum)));
}
