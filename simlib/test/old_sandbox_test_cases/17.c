#include <stdlib.h>

#define ARR_SIZE 1000

// Some recursive function to use up some stack memory
int foo(int n) {
	int arr[ARR_SIZE] = {};
	for (int i = 0; i < ARR_SIZE; ++i) {
		arr[i] = rand() % 3;
		i += rand() % 100;
	}

	int res = 0;
	while (rand() % 3)
		++res;

	if (n > 0)
		res += foo(n - 1);

	res += arr[res % ARR_SIZE];
	return res + (rand() % 3);
}

int main() {
	if (foo(1000000) <= 0)
		abort();
	return 0;
}
