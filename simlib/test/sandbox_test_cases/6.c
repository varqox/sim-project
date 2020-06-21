#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	int fd = open("/tmp", O_RDONLY);
	if (fd == -1)
		abort();
	close(fd);

	return 0;
}
