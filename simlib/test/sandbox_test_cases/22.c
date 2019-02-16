#include <unistd.h>

int main() {
	char buff[5];
	int rc = 0;
	if (read(STDIN_FILENO, buff, 5) < 0)
		rc |= 1;
	if (read(STDOUT_FILENO, buff, 5) < 0)
		rc |= 2;
	if (read(STDERR_FILENO, buff, 5) < 0)
		rc |= 4;

	return rc;
}
