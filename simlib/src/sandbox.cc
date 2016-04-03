#include "../include/sandbox.h"
#include "../include/utilities.h"

#include <climits>
#include <linux/version.h>
#include <sys/uio.h>

using std::array;
using std::string;
using std::vector;

bool Sandbox::DefaultCallback::operator()(pid_t pid, int syscall) {
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

	constexpr array<int, 20> allowed_syscalls_i386 {{
		1, // SYS_exit
		3, // SYS_read
		4, // SYS_write
		6, // SYS_close
		13, // SYS_time
		45, // SYS_brk
		54, // SYS_ioctl
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
	}};

	constexpr array<int, 18> allowed_syscalls_x86_64 {{
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
		60, // SYS_exit
		186, // SYS_gettid
		201, // SYS_time
		231, // SYS_exit_group
		234, // SYS_tgkill
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

	return isSyscallAllowed(pid, arch, syscall, {});
}

bool Sandbox::CallbackBase::isSyscallAllowed(pid_t pid, int arch, int syscall,
	const vector<string>& allowed_files)
{
	constexpr int sys_open[2] = {
		5, // SYS_open - i386
		2 // SYS_open - x86_64
	};

	// SYS_open
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
			sizeof(user_regs)
		};
		if (ptrace(PTRACE_GETREGSET, pid, 1, &ivo) == -1) {
			errlog("Error: ptrace(PTRACE_GETREGS)", error(errno));
			return false;
		}

		if (ARG1 == 0) // NULL is first argument
			return true;

		char path[PATH_MAX] = {};
		ssize_t len;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
		FileDescriptor fd;
		if (fd.open(concat("/proc/", toString(pid), "/mem"),
			O_RDONLY | O_LARGEFILE | O_NOFOLLOW) == -1)
		{
			errlog("Error: open()", error(errno));
			goto null_path;
		}

		if (lseek64(fd, ARG1, SEEK_SET) == -1) {
			errlog("Error: lseek64()", error(errno));
			goto null_path;
		}

		len = read(fd, path, PATH_MAX - 1);
		if (len == -1) {
			errlog("Error: read()", error(errno));
			goto null_path;
		}

#else
		struct iovec local, remote;
		local.iov_base = path;
		remote.iov_base = (void*)ARG1;
		local.iov_len = remote.iov_len = PATH_MAX - 1;

		len = process_vm_readv(pid, &local, 1, &remote, 1, 0);
		if (len == -1) {
			errlog("Error: process_vm_readv()", error(errno));
			goto null_path;
		}
#endif

		path[len] = '\0';
		if (binary_search(allowed_files.begin(), allowed_files.end(), path))
			return true; // File is allowed to open

null_path:
		// Set NULL as first argument to open
		if (arch)
			user_regs.x86_64_regs.rdi = 0;
		else
			user_regs.i386_regs.ebx = 0;

		// Update traced process registers
		ivo.iov_base = &user_regs;
		if (ptrace(PTRACE_SETREGSET, pid, 1, &ivo) == -1) {
			errlog("Error: ptrace(PTRACE_SETREGS)", error(errno));
			return false; // Failed to alter traced process registers
		}

		return true; // Allow to open NULL
	}

	return false;
}
