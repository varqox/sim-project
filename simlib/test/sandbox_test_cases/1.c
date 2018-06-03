#include <sys/syscall.h>
#include <unistd.h>

int main() {
	for (;;)
		syscall(SYS_munmap, 0, 0);
	return 0;
}
