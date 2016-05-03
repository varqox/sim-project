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
		constexpr std::array<int, 5> allowed_syscalls_i386 {{
			19, // SYS_lseek
			41, // SYS_dup
			63, // SYS_dup2
			140, // SYS__llseek
			330, // SYS_dup3
		}};
		constexpr std::array<int, 4> allowed_syscalls_x86_64 {{
			8, // SYS_lseek
			32, // SYS_dup
			33, // SYS_dup2
			292, // SYS_dup3
		}};
		return (DefaultCallback::isSyscallIn(syscall, allowed_syscalls_i386,
				allowed_syscalls_x86_64)
			|| DefaultCallback::isSyscallEntryAllowed(pid, syscall,
				allowed_files));
	}

	using DefaultCallback::detectArchitecture;

	using DefaultCallback::isSyscallExitAllowed;

	using DefaultCallback::errorMessage;
};
