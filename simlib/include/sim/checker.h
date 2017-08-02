#pragma once

#include "../sandbox.h"

namespace sim {

class CheckerCallback : protected Sandbox::DefaultCallback {
	std::vector<std::string> allowed_files; // Sorted list of files allowed to
	                                        // open by the sandboxed program

public:
	explicit CheckerCallback(std::vector<std::string> files = {})
		: allowed_files(files)
	{
		std::sort(allowed_files.begin(), allowed_files.end());
	}

	using DefaultCallback::detect_tracee_architecture;
	using DefaultCallback::get_arch;
	using DefaultCallback::is_syscall_exit_allowed;
	using DefaultCallback::error_message;

	bool is_syscall_entry_allowed(pid_t pid, int syscall) {
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
		return (DefaultCallback::is_syscall_in(syscall, allowed_syscalls_i386,
				allowed_syscalls_x86_64)
			|| DefaultCallback::is_syscall_entry_allowed(pid, syscall,
				allowed_files));
	}
};

/**
 * @brief Obtains checker output (truncated if too long)
 *
 * @param fd file descriptor of file to which checker wrote via stdout. (The
 *   file offset is not changed.)
 * @param max_length maximum length of the returned string
 *
 * @return checker output, truncated if too long
 *
 * @errors Throws an exception of type std::runtime_error with appropriate
 *   message
 */
std::string obtainCheckerOutput(int fd, size_t max_length);

} // namespace sim
