#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
	int fd = open("/tmp", O_RDONLY);
	assert(fd != -1);
	close(fd);

	return 0;
}
