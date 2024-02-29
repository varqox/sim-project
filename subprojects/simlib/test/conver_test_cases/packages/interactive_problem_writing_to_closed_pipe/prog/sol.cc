#include <cassert>
#include <cstdio>

int main() {
	for (;;)
		puts("abc"); // Here we receive SIGPIPE
}
