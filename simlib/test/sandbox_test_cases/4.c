#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

int main() {
	const int page_size = sysconf(_SC_PAGESIZE);
	char* ptr = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE,
	                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	for (int i = 0; i < page_size; ++i)
		ptr[i] = 'x';

	char* fake_name = ptr + page_size - 10;
	if (syscall(SYS_open, fake_name, O_RDONLY) != -1 || errno != ENAMETOOLONG)
		abort();
	if (syscall(SYS_open, NULL, O_RDONLY) != -1 || errno != EFAULT)
		abort();
	if (syscall(SYS_open, "exec", O_RDONLY) != -1 || errno != EPERM)
		abort();

	sigset_t set;
	if (sigfillset(&set))
		abort();
	if (sigprocmask(SIG_BLOCK, &set, NULL))
		abort();
	syscall(SYS_socket, 0, 0, 0);

	return 0;
}
