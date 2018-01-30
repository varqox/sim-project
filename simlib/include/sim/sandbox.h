#pragma once

#include "../sandbox.h"

namespace sim {

class SandboxCallback : protected Sandbox::DefaultCallback {
	std::vector<std::pair<std::string, OpenAccess>> allowed_files; // Sorted
		// list of files (and modes in which they are) allowed to be opened by
		// the sandboxed program

public:
	explicit SandboxCallback(std::vector<std::pair<std::string, OpenAccess>> files = {})
		: allowed_files(files)
	{
		std::sort(allowed_files.begin(), allowed_files.end(),
				[](auto&& a, auto&& b) { return a.first < b.first; });
	}

	using DefaultCallback::detect_tracee_architecture;
	using DefaultCallback::get_arch;
	using DefaultCallback::is_syscall_exit_allowed;
	using DefaultCallback::error_message;

	bool is_syscall_entry_allowed(pid_t pid, int syscall) {
		return DefaultCallback::is_syscall_entry_allowed(pid, syscall,
				allowed_files);
	}
};

} // namespace sim
