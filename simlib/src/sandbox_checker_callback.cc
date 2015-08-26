#include "../include/debug.h"
#include "../include/sandbox_checker_callback.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <unistd.h>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::string;
using std::vector;

struct i386_user_regs_struct {
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t xds;
	uint32_t xes;
	uint32_t xfs;
	uint32_t xgs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t xcs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t xss;
};

struct x86_64_user_regs_struct {
	unsigned long long int r15;
	unsigned long long int r14;
	unsigned long long int r13;
	unsigned long long int r12;
	unsigned long long int rbp;
	unsigned long long int rbx;
	unsigned long long int r11;
	unsigned long long int r10;
	unsigned long long int r9;
	unsigned long long int r8;
	unsigned long long int rax;
	unsigned long long int rcx;
	unsigned long long int rdx;
	unsigned long long int rsi;
	unsigned long long int rdi;
	unsigned long long int orig_rax;
	unsigned long long int rip;
	unsigned long long int cs;
	unsigned long long int eflags;
	unsigned long long int rsp;
	unsigned long long int ss;
	unsigned long long int fs_base;
	unsigned long long int gs_base;
	unsigned long long int ds;
	unsigned long long int es;
	unsigned long long int fs;
	unsigned long long int gs;
};

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
		(192) // SYS_mmap2
		(197) // SYS_fstat64
		(252) // SYS_exit_group
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
		(11) // SYS_munmap
		(12) // SYS_brk
		(16) // SYS_ioctl
		(32) // SYS_dup
		(33) // SYS_dup2
		(60) // SYS_exit
		(231) // SYS_exit_group
		(292); // SYS_dup3
};

int CheckerCallback::operator()(int pid, int syscall) {
	// Detect arch (first call - before exec, second - after exec)
	if (++functor_call < 3) {
		string filename = "/proc/" + toString((long long)pid) + "/exe";

		int fd = open(filename.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			arch = 0;
			E("Error: '%s' - %s\n", filename.c_str(), strerror(errno));
		} else {
			// Read fourth byte and detect if 32 or 64 bit
			unsigned char c;
			lseek(fd, 4, SEEK_SET);

			int ret = read(fd, &c, 1);
			if (ret == 1 && c == 2)
				arch = 1; // x86_64
			else
				arch = 0; // i386

			close(fd);
		}
	}

	// Check if syscall is allowed
	foreach (i, allowed_syscalls[arch])
		if (syscall == *i)
			return 0;

	// Check if syscall is limited
	foreach (i, limited_syscalls[arch])
		if (syscall == i->syscall)
			return --i->limit < 0;

	const int sys_open[2] = {
		5, // SYS_open - i386
		2 // SYS_open - x86_64
	};

	if (syscall == sys_open[arch]) {
		union user_regs_union {
			i386_user_regs_struct i386_regs;
			x86_64_user_regs_struct x86_64_regs;
		} user_regs;

#define USER_REGS (arch ? user_regs.x86_64_regs : user_regs.i386_regs)
#define ARG1 (arch ? user_regs.x86_64_regs.rdi : user_regs.i386_regs.ebx)
#define ARG2 (arch ? user_regs.x86_64_regs.rsi : user_regs.i386_regs.ecx)
#define ARG3 (arch ? user_regs.x86_64_regs.rdx : user_regs.i386_regs.edx)
#define ARG4 (arch ? user_regs.x86_64_regs.r10 : user_regs.i386_regs.esi)
#define ARG5 (arch ? user_regs.x86_64_regs.r8 : user_regs.i386_regs.edi)
#define ARG6 (arch ? user_regs.x86_64_regs.r9 : user_regs.i386_regs.ebp)

		struct iovec ivo = {
			&user_regs,
			sizeof(user_regs),
		};
		if (ptrace(PTRACE_GETREGSET, pid, 1, &ivo) == -1)
			return 1; // Error occurred

		if (ARG1 == 0)
			return 0;

		char path[PATH_MAX] = {};
		struct iovec local, remote;
		local.iov_base = path;
		remote.iov_base = (void*)ARG1;
		local.iov_len = remote.iov_len = PATH_MAX - 1;

		ssize_t len = process_vm_readv(pid, &local, 1, &remote, 1, 0);
		if (len == -1)
			return 1; // Error occurred

		path[len] = '\0';
		return !binary_search(allowed_files.begin(), allowed_files.end(), path);
	}

	return 1;
}

} // namespace sandbox
