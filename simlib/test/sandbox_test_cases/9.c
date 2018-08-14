#include <sys/syscall.h>
#include <unistd.h>

int main() {
	syscall(SYS_set_thread_area, 0);
	return 0;
}
