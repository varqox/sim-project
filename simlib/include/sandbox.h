#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace sandbox {

struct i386_user_regs_struct {
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;
	uint32_t eax;
	uint32_t xds;
	uint32_t xes;
	uint32_t xfs;
	uint32_t xgs;
	uint32_t orig_eax;
	uint32_t eip;
	uint32_t xcs;
	uint32_t eflags;
	uint32_t esp;
	uint32_t xss;
};

struct x86_64_user_regs_struct {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t orig_rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t eflags;
	uint64_t rsp;
	uint64_t ss;
	uint64_t fs_base;
	uint64_t gs_base;
	uint64_t ds;
	uint64_t es;
	uint64_t fs;
	uint64_t gs;
};

struct ExitStat {
	int code;
	unsigned long long runtime; // in usec
	std::string message;

	ExitStat(int _1 = 0, unsigned long long _2 = 0, const std::string& _3 = "")
			: code(_1), runtime(_2), message(_3) {}
};

/**
 * @brief Invokes ptr as function with parameters @p a, @p b
 *
 * @param a first parameter
 * @param b second parameter
 * @param ptr pointer to function
 * @tparam C type to convert ptr to before invocation
 */
template<class C>
inline int magic_convert(int a, int b, void *ptr) {
	return (*((C*)ptr))(a, b); // function invocation
}

struct DefaultCallback {
	struct Pair {
		int syscall;
		int limit;
	};

	int functor_call, arch; // arch - architecture: 0 - i386, 1 - x86_64
	std::vector<Pair> limited_syscalls[2]; // 0 - i386, 1 - x86_64
	std::vector<int> allowed_syscalls[2]; // 0 - i386, 1 - x86_64

	DefaultCallback();

	// Returns 0 on success, non-zero value on error
	int operator()(pid_t pid, int syscall);
};

/**
 * @brief Invokes ptr as function with parameters @p a, @p b
 *
 * @param pid traced process (via ptrace) pid
 * @param arch architecture: 0 - i386, 1 - x86_64
 * @param syscall currently invoked syscall
 * @param allowed_filed files which can be opened
 * @return true if call is allowed, false otherwise
 */
bool allowedCall(pid_t pid, int arch, int syscall,
	const std::vector<std::string>& allowed_files);

struct options {
	unsigned long long time_limit; // in usec
	unsigned long long memory_limit; // in bytes
	int new_stdin_fd; // negative - close, STDIN_FILENO - do not change
	int new_stdout_fd; // negative - close, STDOUT_FILENO - do not change
	int new_stderr_fd; // negative - close, STDERR_FILENO - do not change
};

/**
 * @brief Runs @p exec with arguments @p args with limits @p opts->time_limit
 * and @p opts->memory_limit under ptrace(2)
 * @details @p func is called on every syscall entry called by exec with
 * parameters: child pid, syscall number, @p data.
 * @p func must return 0 - syscall is allowed  non-zero
 * @p exec is called via execvp()
 * This function is not thread-safe
 * As of now, the executed program must have the same architecture as its parent
 *
 * @param exec file that is to be executed
 * @param args arguments passed to exec
 * @param opts options (time_limit set to 0 disables time limit,
 * memory_limit set to 0 disables memory limit,
 * new_stdin_fd, new_stdout_fd, new_stderr_fd - file descriptors to which
 * respectively stdin, stdout, stderr of child process will be changed or if
 * negative, closed)
 * @param func pointer to callback function
 * @param data pointer which will be passed to @p func as last argument
 * @return Returns ExitStat structure with fields: code is -1 on error, or
 * return status (in the format specified in wait(2)).
 */
ExitStat run(const std::string& exec, std::vector<std::string> args,
		const struct options *opts, int (*func)(int, int, void*), void *data);

/**
 * @brief Alias to run()
 */
inline ExitStat run(const std::string& exec,
		std::vector<std::string> args, const struct options *opts) {

	DefaultCallback dc;
	return run(exec, args, opts, magic_convert<DefaultCallback>, (void*)&dc);
}

/**
 * @brief Alias to run()
 */
inline ExitStat run(const std::string& exec, std::vector<std::string> args,
		const struct options *opts,
		int (*func)(int, int)) {

	return run(exec, args, opts, magic_convert<int (int,int)>, (void*)func);
}

/**
 * @brief Alias to run()
 */
template<class C>
inline ExitStat run(const std::string& exec, std::vector<std::string> args,
		const struct options *opts,	const C& func) {

	return run(exec, args, opts, magic_convert<C>, (void*)&func);
}

/**
 * @brief Like run() but thread-safe
 */
ExitStat thread_safe_run(const std::string& exec, std::vector<std::string> args,
		const struct options *opts,	int (*func)(int, int, void*), void *data);

/**
 * @brief Like run() but thread-safe
 */
inline ExitStat thread_safe_run(const std::string& exec,
		std::vector<std::string> args, const struct options *opts) {

	DefaultCallback dc;
	return thread_safe_run(exec, args, opts, magic_convert<DefaultCallback>,
		(void*)&dc);
}

/**
 * @brief Like run() but thread-safe
 */
inline ExitStat thread_safe_run(const std::string& exec,
		std::vector<std::string> args, const struct options *opts,
		int (*func)(int, int)) {

	return thread_safe_run(exec, args, opts, magic_convert<int (int,int)>,
		(void*)func);
}

/**
 * @brief Like run() but thread-safe
 */
template<class C>
inline ExitStat thread_safe_run(const std::string& exec,
		std::vector<std::string> args, const struct options *opts,
		const C& func) {

	return thread_safe_run(exec, args, opts, magic_convert<C>, (void*)&func);
}

} // namespace sandbox
