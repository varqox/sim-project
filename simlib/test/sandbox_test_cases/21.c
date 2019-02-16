#include <unistd.h>

int main() {
	int rc = 0;
	if (write(STDIN_FILENO, "test\n", 5) != 5)
		rc |= 1;
	if (write(STDOUT_FILENO, "test\n", 5) != 5)
		rc |= 2;
	if (write(STDERR_FILENO, "test\n", 5) != 5)
		rc |= 4;

	return rc;
}
