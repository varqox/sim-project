#include <assert.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ARR_SIZE 10 << 20

// Some function that uses ARR_SIZE bytes of memory in a tricky way to prevent
// compiler from optimizing away the memory usage
int foo() {
	char arr[ARR_SIZE] = {};
	for (int i = 0; i < ARR_SIZE; ++i) {
		arr[i] = rand() % 3;
		i += rand() % 100;
	}

	int res = 0;
	while (rand() % 3)
		++res;

	return res + arr[res % ARR_SIZE];
}

int main() {
	assert(foo() > 0);

	for (;;)
		syscall(SYS_munmap, 0, 0);

	return 0;
}
