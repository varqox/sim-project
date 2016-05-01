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
		constexpr std::array<int, 2> allowed_syscalls_i386 {{
			19, // SYS_lseek
			140, // SYS__llseek
		}};
		constexpr std::array<int, 1> allowed_syscalls_x86_64 {{
			8, // SYS_lseek
		}};
		stdlog("Called checker callback!");
		return (DefaultCallback::isSyscallEntryAllowed(pid, syscall,
				allowed_files)
			|| DefaultCallback::isSyscallIn(syscall, allowed_syscalls_i386,
				allowed_syscalls_x86_64));
	}

	using DefaultCallback::isSyscallExitAllowed;

	using DefaultCallback::errorMessage;
};
