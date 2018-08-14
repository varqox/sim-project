#include <sys/syscall.h>
#include <unistd.h>

int main() {
	syscall(SYS_uname, 0);
	return 0;
}
