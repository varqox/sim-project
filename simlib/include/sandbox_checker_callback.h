#pragma once

#include "sandbox.h"

class CheckerCallback : protected Sandbox::DefaultCallback {
	std::vector<std::string> allowed_files; // sorted list of files allowed to

public:
	explicit CheckerCallback(std::vector<std::string> files = {})
		: allowed_files(files)
	{
		std::sort(allowed_files.begin(), allowed_files.end());
	}

	bool isSyscallEntryAllowed(pid_t pid, int syscall) {
		constexpr std::array<int, 23> allowed_syscalls_i386 {{
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
		constexpr std::array<int, 21> allowed_syscalls_x86_64 {{
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

		return DefaultCallback::isSyscallEntryAllowed(pid, syscall,
			allowed_syscalls_i386, allowed_syscalls_x86_64, allowed_files);
	}

	using DefaultCallback::isSyscallExitAllowed;

	using DefaultCallback::errorMessage;
};
