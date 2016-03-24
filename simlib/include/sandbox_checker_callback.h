#pragma once

#include "sandbox.h"

struct CheckerCallback : Sandbox::CallbackBase {
	struct Pair {
		int syscall;
		int limit;
	};

	int functor_call;
	int8_t arch; // arch - architecture: 0 - i386, 1 - x86_64
	std::vector<std::string> allowed_files; // sorted list of files allowed to
	                                        // open
	std::vector<Pair> limited_syscalls[2] = {
		{ /* i386 */
			{  11, 1 }, // SYS_execve
			{  33, 1 }, // SYS_access
			{  85, 1 }, // SYS_readlink
			{ 122, 1 }, // SYS_uname
			{ 243, 1 }, // SYS_set_thread_area
		},
		{ /* x86_64 */
			{  21, 1 }, // SYS_access
			{  59, 1 }, // SYS_execve
			{  63, 1 }, // SYS_uname
			{  89, 1 }, // SYS_readlink
			{ 158, 1 }, // SYS_arch_prctl
			{ 205, 1 }, // SYS_set_thread_area
		}
	};

	explicit CheckerCallback(std::vector<std::string> files = {})
		: functor_call(0), arch(-1), allowed_files(files)
	{
		std::sort(allowed_files.begin(), allowed_files.end());
	}

	bool operator()(pid_t pid, int syscall);
};
