#include <sys/syscall.h>
#include <unistd.h>

int main() {
	syscall(SYS_execve, 0, 0, 0);
	return 0;
}
