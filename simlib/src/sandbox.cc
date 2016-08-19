#include "../include/sandbox.h"

#include <linux/version.h>
#include <sys/uio.h>

using std::array;
using std::string;
using std::vector;

#define USER_REGS (arch ? regs.uregs.x86_64_regs : regs.uregs.i386_regs)
#define RES (arch ? regs.uregs.x86_64_regs.rax : regs.uregs.i386_regs.eax)
#define ARG1 (arch ? regs.uregs.x86_64_regs.rdi : regs.uregs.i386_regs.ebx)
#define ARG2 (arch ? regs.uregs.x86_64_regs.rsi : regs.uregs.i386_regs.ecx)
#define ARG3 (arch ? regs.uregs.x86_64_regs.rdx : regs.uregs.i386_regs.edx)
#define ARG4 (arch ? regs.uregs.x86_64_regs.r10 : regs.uregs.i386_regs.esi)
#define ARG5 (arch ? regs.uregs.x86_64_regs.r8 : regs.uregs.i386_regs.edi)
#define ARG6 (arch ? regs.uregs.x86_64_regs.r9 : regs.uregs.i386_regs.ebp)

bool Sandbox::CallbackBase::isSysOpenAllowed(pid_t pid,
	const vector<string>& allowed_files)
{
	Registers regs;
	regs.getRegs(pid);

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
		regs.uregs.x86_64_regs.rdi = 0;
	else
		regs.uregs.i386_regs.ebx = 0;

	regs.setRegs(pid); // Update traced process registers

	return true; // Allow to open NULL
}

bool Sandbox::CallbackBase::isSysLseekAllowed(pid_t pid) {
	Registers regs;
	regs.getRegs(pid);

	// Disallow lseek on stdin, stdout and stderr
	array<int, 3> stdfiles {{STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}};
	if (std::find(stdfiles.begin(), stdfiles.end(), ARG1) == stdfiles.end())
		return true;

	// Set -1 as first argument to lseek
	if (arch)
		regs.uregs.x86_64_regs.rdi = -1;
	else
		regs.uregs.i386_regs.ebx = -1;

	regs.setRegs(pid); // Update traced process registers

	return true; // Allow to lseek -1
}

bool Sandbox::CallbackBase::isSysTgkillAllowed(pid_t pid) {
	Registers regs;
	regs.getRegs(pid);

	// The thread (one in the process) is allowed to kill itself ONLY
	return (ARG1 == (uint64_t)pid && ARG2 == (uint64_t)pid);
}

bool Sandbox::DefaultCallback::isSyscallExitAllowed(pid_t pid, int syscall) {
	constexpr int sys_execve[2] = {
		11, // SYS_execve - i386
		59, // SYS_execve - x86_64
	};
	constexpr int sys_execveat[2] = {
		358, // SYS_execve - i386
		322, // SYS_execve - x86_64
	};
	constexpr int sys_brk[2] = {
		45, // SYS_brk - i386
		12, // SYS_brk - x86_64
	};

	if (syscall == sys_execve[arch] || syscall == sys_execveat[arch])
		detectArchitecture(pid);

	if (syscall != sys_brk[arch])
		return true;

	/* syscall == SYS_brk */

	/* This is needed to prevent tracee going over the time limit when it is
	 * flooding kernel with unsuccessful syscalls asking for more memory. No
	 * normal program would constantly unsuccessfully ask for memory. Because of
	 * that, instead of time out you will get "Memory limit exceeded". All this
	 * is done to improve readability of ExitStatus.
	 */

	Registers regs;
	regs.getRegs(pid);

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
