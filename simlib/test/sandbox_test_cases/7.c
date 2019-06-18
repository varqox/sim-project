#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
	int fd = open("/dev/null", O_RDONLY);
	assert(fd != -1);

	if (dup2(fd, STDIN_FILENO) == -1)
		return __LINE__;
	if (dup2(fd, STDOUT_FILENO) == -1)
		return __LINE__;
	if (dup2(fd, STDERR_FILENO) == -1)
		return __LINE__;

	if (dup3(fd, STDIN_FILENO, 0) != -1 || errno != EINVAL)
		return __LINE__;
	if (dup3(fd, STDOUT_FILENO, 0) == -1)
		return __LINE__;
	if (dup3(fd, STDERR_FILENO, 0) == -1)
		return __LINE__;

	int fd2 = open("/dev/null", O_RDONLY);

	if (dup2(fd2, STDIN_FILENO) == -1)
		return __LINE__;
	if (dup2(fd2, STDOUT_FILENO) == -1)
		return __LINE__;
	if (dup2(fd2, STDERR_FILENO) == -1)
		return __LINE__;

	if (dup3(fd2, STDIN_FILENO, 0) == -1)
		return __LINE__;
	if (dup3(fd2, STDOUT_FILENO, 0) == -1)
		return __LINE__;
	if (dup3(fd2, STDERR_FILENO, 0) == -1)
		return __LINE__;

	close(fd2);
	close(fd);

	return 0;
}
