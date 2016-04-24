#include "../include/sandbox.h"
#include "../include/utilities.h"

#include <linux/version.h>

using std::array;
using std::string;
using std::vector;

#define USER_REGS (arch ? user_regs.x86_64_regs : user_regs.i386_regs)
#define RES (arch ? user_regs.x86_64_regs.rax : user_regs.i386_regs.eax)
#define ARG1 (arch ? user_regs.x86_64_regs.rdi : user_regs.i386_regs.ebx)
#define ARG2 (arch ? user_regs.x86_64_regs.rsi : user_regs.i386_regs.ecx)
#define ARG3 (arch ? user_regs.x86_64_regs.rdx : user_regs.i386_regs.edx)
#define ARG4 (arch ? user_regs.x86_64_regs.r10 : user_regs.i386_regs.esi)
#define ARG5 (arch ? user_regs.x86_64_regs.r8 : user_regs.i386_regs.edi)
#define ARG6 (arch ? user_regs.x86_64_regs.r9 : user_regs.i386_regs.ebp)

bool Sandbox::CallbackBase::isSysOpenAllowed(pid_t pid, int arch,
	const vector<string>& allowed_files)
{
	union user_regs_union {
		i386_user_regset i386_regs;
		x86_64_user_regset x86_64_regs;
	} user_regs;

	struct iovec ivo = {
		&user_regs,
		sizeof(user_regs)
	};
	if (ptrace(PTRACE_GETREGSET, pid, 1, &ivo) == -1)
		THROW("Error: ptrace(PTRACE_GETREGS)", error(errno));

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
	if (ptrace(PTRACE_SETREGSET, pid, 1, &ivo) == -1)
		THROW("Error: ptrace(PTRACE_SETREGS)", error(errno));

	return true; // Allow to open NULL
}

bool Sandbox::DefaultCallback::isSyscallExitAllowed(pid_t pid, int syscall) {
	constexpr int sys_brk[2] = {
		45, // SYS_brk - i386
		12, // SYS_brk - x86_64
	};
	if (syscall != sys_brk[arch])
		return true;

	/* syscall == SYS_brk */

	union user_regs_union {
		i386_user_regset i386_regs;
		x86_64_user_regset x86_64_regs;
	} user_regs;

	struct iovec ivo = {
		&user_regs,
		sizeof(user_regs)
	};
	if (ptrace(PTRACE_GETREGSET, pid, 1, &ivo) == -1)
		THROW("Error: ptrace(PTRACE_GETREGS)", error(errno));

	// Count unsuccessful series of SYS_brk calls
	if (ARG1 <= RES) {
		unsuccessful_SYS_brk_counter = 0;
		return true;
	}

	if (++unsuccessful_SYS_brk_counter < UNSUCCESSFUL_SYS_BRK_LIMIT)
		return true;

	error_message = "Memory limit exceeded";
	return false;
}
