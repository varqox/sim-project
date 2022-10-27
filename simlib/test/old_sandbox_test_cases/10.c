#include <sys/syscall.h>
#include <unistd.h>

int main() {
	syscall(SYS_arch_prctl, 0, 0);
	return 0;
}
