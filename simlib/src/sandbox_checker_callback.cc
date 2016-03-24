#include "../include/sandbox_checker_callback.h"
#include "../include/utilities.h"

using std::array;
using std::string;
using std::vector;

bool CheckerCallback::operator()(pid_t pid, int syscall) {
	// Detect arch (first call - before exec, second - after exec)
	if (++functor_call < 3) {
		string filename = concat("/proc/", toString<int64_t>(pid), "/exe");

		int fd = open(filename.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			arch = 0;
			errlog("Error: open('", filename, "')", error(errno));
			return false;
		}

		Closer closer(fd);
		// Read fourth byte and detect if 32 or 64 bit
		unsigned char c;
		if (lseek(fd, 4, SEEK_SET) == (off_t)-1)
			return false;

		int ret = read(fd, &c, 1);
		if (ret == 1 && c == 2)
			arch = 1; // x86_64
		else
			arch = 0; // i386
	}

	constexpr array<int, 23> allowed_syscalls_i386 {{
		1, // SYS_exit
		3, // SYS_read
		4, // SYS_write
		6, // SYS_close
		13, // SYS_time
		41, // SYS_dup
		45, // SYS_brk
		54, // SYS_ioctl
		63, // SYS_dup2
		90, // SYS_mmap
		91, // SYS_munmap
		108, // SYS_fstat
		125, // SYS_mprotect
		145, // SYS_readv
		146, // SYS_writev
		174, // SYS_rt_sigaction
		175, // SYS_rt_sigprocmask
		192, // SYS_mmap2
		197, // SYS_fstat64
		224, // SYS_gettid
		252, // SYS_exit_group
		270, // SYS_tgkill
		330, // SYS_dup3
	}};
	constexpr array<int, 21> allowed_syscalls_x86_64 {{
		0, // SYS_read
		1, // SYS_write
		3, // SYS_close
		5, // SYS_fstat
		9, // SYS_mmap
		10, // SYS_mprotect
		11, // SYS_munmap
		12, // SYS_brk
		13, // SYS_rt_sigaction
		14, // SYS_rt_sigprocmask
		16, // SYS_ioctl
		19, // SYS_readv
		20, // SYS_writev
		32, // SYS_dup
		33, // SYS_dup2
		60, // SYS_exit
		186, // SYS_gettid
		201, // SYS_time
		231, // SYS_exit_group
		234, // SYS_tgkill
		292, // SYS_dup3
	}};

	// Check if syscall is allowed
	if (arch == 0) {
		if (binary_search(allowed_syscalls_i386, syscall))
			return true;
	} else {
		if (binary_search(allowed_syscalls_x86_64, syscall))
			return true;
	}

	// Check if syscall is limited
	for (Pair& i : limited_syscalls[arch])
		if (syscall == i.syscall)
			return --i.limit >= 0;

	return isSyscallAllowed(pid, arch, syscall, allowed_files);
}
