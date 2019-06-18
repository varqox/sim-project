#include <assert.h>
#include <signal.h>
#include <stdlib.h>

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
	sigset_t set;
	assert(0 == sigfillset(&set));
	assert(0 == sigprocmask(SIG_BLOCK, &set, NULL));
	assert(foo() > 0);

	for (;;) {
	}
	return 0;
}
