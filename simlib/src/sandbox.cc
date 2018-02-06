#include "../include/sandbox.h"

#include <linux/version.h>
#include <sys/uio.h>

using std::array;
using std::pair;
using std::string;
using std::vector;

bool Sandbox::CallbackBase::is_open_allowed(pid_t pid,
	const vector<pair<string, OpenAccess>>& allowed_files)
{
	SyscallRegisters regs(pid, arch_);

	if (regs.arg1u() == 0) // nullptr is the first argument
		return true;

	char path[PATH_MAX] = {};
	ssize_t len;

	auto set_arg1_to_null = [&] {
		regs.set_arg1u(0); // Set nullptr as the first argument to open
		regs.set_regs(pid); // Update traced process's registers
	};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
	FileDescriptor fd;
	if (fd.open(concat("/proc/", pid, "/mem").to_cstr(),
		O_RDONLY | O_LARGEFILE | O_NOFOLLOW) == -1)
	{
		errlog("Error: open()", errmsg());
		set_arg1_to_null();
		return true; // Allow to open nullptr
	}

	len = pread64(fd, path, PATH_MAX - 1, regs.arg1u());
	if (len == -1) {
		errlog("Error: pread64()", errmsg());
		set_arg1_to_null();
		return true; // Allow to open nullptr
	}

#else
	struct iovec local, remote;
	local.iov_base = path;
	remote.iov_base = (void*)regs.arg1u();
	local.iov_len = remote.iov_len = PATH_MAX - 1;

	len = process_vm_readv(pid, &local, 1, &remote, 1, 0);
	if (len == -1) {
		errlog("Error: process_vm_readv()", errmsg());
		set_arg1_to_null();
		return true; // Allow to open nullptr
	}
#endif

	path[len] = '\0';
	auto it =
		binaryFindBy(allowed_files, &pair<string, OpenAccess>::first, path);
	if (it == allowed_files.end()) {
		set_arg1_to_null();
		return true; // Allow to open nullptr
	}

	auto arg2 = regs.arg2u();
	OpenAccess open_mode = it->second;

	if ((arg2 & O_RDONLY) == O_RDONLY) {
		if (open_mode == OpenAccess::RDONLY or open_mode == OpenAccess::RDWR)
			return true; // File is allowed to be opened
	} else if ((arg2 & O_WRONLY) == O_WRONLY) {
		if (open_mode == OpenAccess::WRONLY or open_mode == OpenAccess::RDWR)
			return true; // File is allowed to be opened
	} else if ((arg2 & O_RDWR) == O_RDWR and open_mode == OpenAccess::RDWR)
		return true; // File is allowed to be opened

	set_arg1_to_null();
	return true; // Allow to open nullptr
}

enum class LseekOrDupData {
	ORGINAL_ARG1,
	SUBSTITUTED_ARG1
};

bool Sandbox::CallbackBase::is_lseek_or_dup_allowed(pid_t pid) {
	SyscallRegisters regs(pid, arch_);

	// Disallow lseek on stdin, stdout and stderr
	array<int, 3> stdfiles {{STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}};
	if (std::find(stdfiles.begin(), stdfiles.end(), regs.arg1i()) ==
		stdfiles.end())
	{
		reinterpret_cast<LseekOrDupData&>(data_) = LseekOrDupData::ORGINAL_ARG1;
		return true;
	}

	reinterpret_cast<LseekOrDupData&>(data_) = LseekOrDupData::SUBSTITUTED_ARG1;
	regs.set_arg1i(-1); // Set -1 as the first argument to lseek
	regs.set_regs(pid); // Update traced process's registers

	return true; // Allow to lseek on -1
}

bool Sandbox::CallbackBase::is_sysinfo_allowed(pid_t pid) {
	SyscallRegisters regs(pid, arch_);

	regs.set_arg1u(0); // Set NULL as the first argument to sysinfo
	regs.set_regs(pid); // Update traced process's registers

	return true; // Allow sysinfo on NULL
}

bool Sandbox::CallbackBase::is_tgkill_allowed(pid_t pid) {
	SyscallRegisters regs(pid, arch_);

	// The thread (one in the process) is allowed to kill itself ONLY
	return (regs.arg1u() == (uint64_t)pid && regs.arg2u() == (uint64_t)pid);
}

bool Sandbox::DefaultCallback::is_syscall_exit_allowed(pid_t pid, int syscall) {
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

	if (syscall == sys_execve[arch_] || syscall == sys_execveat[arch_])
		detect_tracee_architecture(pid);

	// If lseek changed the fd argument to -1 then set error to ESPIPE (pretend
	// stdin, stdout and stderr to be a PIPE)
	if (is_syslseek(syscall) and reinterpret_cast<LseekOrDupData&>(data_) ==
		LseekOrDupData::SUBSTITUTED_ARG1)
	{
		SyscallRegisters regs(pid, arch_);
		regs.set_resi(-ESPIPE);
		regs.set_regs(pid);
		return true;
	}

	if (syscall != sys_brk[arch_])
		return true;

	/* syscall == SYS_brk */

	/* This is needed to prevent tracee going over the time limit when it is
	 * flooding kernel with unsuccessful syscalls asking for more memory. No
	 * normal program would constantly unsuccessfully ask for memory. Because of
	 * that, instead of time out you will get "Memory limit exceeded". All this
	 * is done to improve readability of ExitStatus.
	 */

	SyscallRegisters regs(pid, arch_);

	// Count unsuccessful series of SYS_brk calls
	if (regs.arg1u() <= regs.resu()) {
		unsuccessful_SYS_brk_counter = 0;
		return true;
	}

	if (++unsuccessful_SYS_brk_counter < UNSUCCESSFUL_SYS_BRK_LIMIT)
		return true;

	error_message_ = "Memory limit exceeded";
	return false;
}
