#pragma once

#include <string>
#include <vector>

namespace sandbox {

struct CheckerCallback {
	struct Pair {
		int syscall;
		int limit;
	};

	int functor_call, arch; // arch - architecture: 0 - i386, 1 - x86_64
	std::vector<std::string> allowed_files; // sorted list of files allowed to
	                                        // open
	std::vector<Pair> limited_syscalls[2]; // 0 - i386, 1 - x86_64
	std::vector<int> allowed_syscalls[2]; // 0 - i386, 1 - x86_64

	CheckerCallback(
		std::vector<std::string> files = std::vector<std::string>());

	int operator()(pid_t pid, int syscall);
};

} // namespace sandbox
