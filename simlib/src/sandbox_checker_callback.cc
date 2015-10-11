#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/logger.h"
#include "../include/sandbox.h"
#include "../include/sandbox_checker_callback.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <unistd.h>

using std::string;
using std::vector;

namespace sandbox {

CheckerCallback::CheckerCallback(vector<string> files)
		: functor_call(0), arch(-1), allowed_files(files) {
	std::sort(allowed_files.begin(), allowed_files.end());
	// i386
	append(limited_syscalls[0])
		((Pair){  11, 1 }) // SYS_execve
		((Pair){  33, 1 }) // SYS_access
		((Pair){  85, 1 }) // SYS_readlink
		((Pair){ 122, 1 }) // SYS_uname
		((Pair){ 243, 1 }); // SYS_set_thread_area

	append(allowed_syscalls[0])
		(1) // SYS_exit
		(3) // SYS_read
		(4) // SYS_write
		(6) // SYS_close
		(41) // SYS_dup
		(45) // SYS_brk
		(54) // SYS_ioctl
		(63) // SYS_dup2
		(90) // SYS_mmap
		(91) // SYS_munmap
		(108) // SYS_fstat
		(125) // SYS_mprotect
		(145) // SYS_readv
		(146) // SYS_writev
		(174) // SYS_rt_sigaction
		(175) // SYS_rt_sigprocmask
		(192) // SYS_mmap2
		(197) // SYS_fstat64
		(224) // SYS_gettid
		(252) // SYS_exit_group
		(270) // SYS_tgkill
		(330); // SYS_dup3

	// x86_64
	append(limited_syscalls[1])
		((Pair){  21, 1 }) // SYS_access
		((Pair){  59, 1 }) // SYS_execve
		((Pair){  63, 1 }) // SYS_uname
		((Pair){  89, 1 }) // SYS_readlink
		((Pair){ 205, 1 }) // SYS_set_thread_area
		((Pair){ 158, 1 }); // SYS_arch_prctl

	append(allowed_syscalls[1])
		(0) // SYS_read
		(1) // SYS_write
		(3) // SYS_close
		(5) // SYS_fstat
		(9) // SYS_mmap
		(10) // SYS_mprotect
		(11) // SYS_munmap
		(12) // SYS_brk
		(13) // SYS_rt_sigaction
		(14) // SYS_rt_sigprocmask
		(16) // SYS_ioctl
		(19) // SYS_readv
		(20) // SYS_writev
		(32) // SYS_dup
		(33) // SYS_dup2
		(60) // SYS_exit
		(186) // SYS_gettid
		(231) // SYS_exit_group
		(234) // SYS_tgkill
		(292); // SYS_dup3
};

int CheckerCallback::operator()(pid_t pid, int syscall) {
	// Detect arch (first call - before exec, second - after exec)
	if (++functor_call < 3) {
		string filename = "/proc/" + toString((long long)pid) + "/exe";

		int fd = open(filename.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			arch = 0;
			error_log("Error: open('", filename, "')", error(errno));
			return 1;

		} else {
			// Read fourth byte and detect if 32 or 64 bit
			unsigned char c;
			if (lseek(fd, 4, SEEK_SET) == (off_t)-1) {
				sclose(fd);
				return 1;
			}

			int ret = read(fd, &c, 1);
			if (ret == 1 && c == 2)
				arch = 1; // x86_64
			else
				arch = 0; // i386

			sclose(fd);
		}
	}

	// Check if syscall is allowed
	for (auto& i : allowed_syscalls[arch])
		if (syscall == i)
			return 0;

	// Check if syscall is limited
	for (auto& i : limited_syscalls[arch])
		if (syscall == i.syscall)
			return --i.limit < 0;

	return !allowedCall(pid, arch, syscall, allowed_files);
}

} // namespace sandbox
