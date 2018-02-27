#pragma once

#include "meta.h"
#include "process.h"
#include "spawner.h"
#include "syscall_name.h"
#include "utilities.h"

#include <cstddef>
#include <sys/ptrace.h>
#include <sys/syscall.h>

#if 0
# warning "Before committing disable this debug"
# define DEBUG_SANDBOX(...) __VA_ARGS__
#else
# define DEBUG_SANDBOX(...)
#endif

enum class OpenAccess {
	RDONLY,
	WRONLY,
	RDWR
};

class Sandbox : protected Spawner {
public:
	Sandbox() = delete;

	struct i386_user_regset;

	struct x86_64_user_regset;

public:
	class CallbackBase {
	protected:
		friend class Sandbox;
		struct Registers;
		struct SyscallRegisters;

		int8_t arch_ = -1; // arch_ - architecture: 0 - i386, 1 - x86_64


#if __cplusplus > 201402L
#warning "Since C++17 std::any or std::variant should be used"
#endif
        char data_[64]; // A place to store information for the syscall checking
                        // functions

		/**
		 * @brief Checks whether syscall open(2) is allowed, if not, tries to
		 *   modify it (by replacing filename with nullptr) so that it will fail
		 *
		 * @param pid pid of traced process (via ptrace)
		 * @param allowed_filed files and mode in which they are allowed to be
		 *   opened
		 *
		 * @return true if call is allowed (modified if needed), false otherwise
		 */
		bool is_open_allowed(pid_t pid,
			const std::vector<std::pair<std::string, OpenAccess>>& allowed_files = {});

		/**
		 * @brief Checks whether syscall lseek(2), dup(2), dup2(2), or dup3(2)
		 *   is allowed, if not, tries to modify it (by replacing fd with -1) so
		 *   that it will fail
		 *
		 * @param pid pid of traced process (via ptrace)
		 *
		 * @return true if call is allowed (modified if needed), false otherwise
		 */
		bool is_lseek_or_dup_allowed(pid_t pid);

		/**
		 * @brief Checks whether syscall sysinfo(2) is allowed, if not, tries to
		 *   modify it (by replacing info with NULL) so that it will fail
		 *
		 * @param pid pid of traced process (via ptrace)
		 *
		 * @return true if call is allowed (modified if needed), false otherwise
		 */
		bool is_sysinfo_allowed(pid_t pid);

		/**
		 * @brief Checks whether syscall tgkill(2) is allowed
		 *
		 * @details Syscall is allowed only if the first and the second argument
		 *   are equal to @p pid
		 *
		 * @param pid pid of traced process (via ptrace)
		 *
		 * @return true if call is allowed, false otherwise
		 */
		bool is_tgkill_allowed(pid_t pid);

	public:
		void detect_tracee_architecture(pid_t pid) {
			arch_ = ::detectArchitecture(pid);
		}

		int8_t get_arch() const noexcept { return arch_; }

		/**
		 * @brief Checks whether or not entering syscall @p syscall is allowed
		 *
		 * @param pid sandboxed process id
		 * @param syscall executed syscall number to check
		 *
		 * @return true if syscall is allowed to be executed, false otherwise
		 */
		virtual bool is_syscall_entry_allowed(pid_t pid, int syscall) = 0;

		/**
		 * @brief Checks whether or not exit from finished syscall @p syscall
		 *   is allowed
		 *
		 * @param pid sandboxed process id
		 * @param syscall executed syscall number to check
		 *
		 * @return true if finished syscall is allowed to exit, false otherwise
		 */
		virtual bool is_syscall_exit_allowed(pid_t pid, int syscall) = 0;

		/**
		 * @brief Returns error message which was set after unsuccessful call to
		 *   either is_syscall_entry_allowed() or is_syscall_exit_allowed()
		 * @return an error message
		 */
		virtual std::string error_message() const = 0;

		virtual ~CallbackBase() noexcept {}
	};

	class DefaultCallback : public CallbackBase {
	protected:
		struct Pair {
			int syscall;
			int limit;
		};

		static_assert(ARCH_i386 == 0, "Invalid value of a constant");
		static_assert(ARCH_x86_64 == 1, "Invalid value of a constant");

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

		int unsuccessful_SYS_brk_counter = 0; // used in
		                                      // is_syscall_exit_allowed()
		static constexpr int UNSUCCESSFUL_SYS_BRK_LIMIT = 128;

		std::string error_message_;

		/// Check whether syscall @p syscall lies in proper syscall_list
		template<size_t N1, size_t N2>
		bool is_syscall_in(int syscall,
			const std::array<int, N1>& syscall_list_i386,
			const std::array<int, N2>& syscall_list_x86_64) const noexcept;

		/// Check whether syscall @p syscall is allowed
		/// @param allowed_filed files and mode in which they are allowed to be
		///   opened
		template<size_t N1, size_t N2>
		bool is_syscall_entry_allowed(pid_t pid, int syscall,
			const std::array<int, N1>& allowed_syscalls_i386,
			const std::array<int, N2>& allowed_syscalls_x86_64,
			const std::vector<std::pair<std::string, OpenAccess>>& allowed_files);

		/// @param allowed_filed files and mode in which they are allowed to be
		///   opened
		bool is_syscall_entry_allowed(pid_t pid, int syscall,
			const std::vector<std::pair<std::string, OpenAccess>>& allowed_files)
		{
			// TODO: make the below syscall numbers more portable
			constexpr std::array<int, 78> allowed_syscalls_i386 {{
				1, // SYS_exit
				3, // SYS_read
				4, // SYS_write
				6, // SYS_close
				13, // SYS_time
				20, // SYS_getpid
				24, // SYS_getuid
				27, // SYS_alarm
				29, // SYS_pause
				45, // SYS_brk
				47, // SYS_getgid
				49, // SYS_geteuid
				50, // SYS_getegid
				67, // SYS_sigaction
				72, // SYS_sigsuspend
				73, // SYS_sigpending
				76, // SYS_getrlimit
				77, // SYS_getrusage
				78, // SYS_gettimeofday
				82, // SYS_select
				90, // SYS_mmap
				91, // SYS_munmap
				100, // SYS_fstatfs
				108, // SYS_fstat
				118, // SYS_fsync
				125, // SYS_mprotect
				142, // SYS__newselect
				143, // SYS_flock
				144, // SYS_msync
				145, // SYS_readv
				146, // SYS_writev
				148, // SYS_fdatasync
				150, // SYS_mlock
				151, // SYS_munlock
				152, // SYS_mlockall
				153, // SYS_munlockall
				162, // SYS_nanosleep
				163, // SYS_mremap
				168, // SYS_poll
				174, // SYS_rt_sigaction
				175, // SYS_rt_sigprocmask
				176, // SYS_rt_sigpending
				177, // SYS_rt_sigtimedwait
				179, // SYS_rt_sigsuspend
				180, // SYS_pread64
				181, // SYS_pwrite64
				184, // SYS_capget
				187, // SYS_sendfile
				191, // SYS_ugetrlimit
				192, // SYS_mmap2
				197, // SYS_fstat64
				199, // SYS_getuid32
				200, // SYS_getgid32
				201, // SYS_geteuid32
				202, // SYS_getegid32
				219, // SYS_madvise
				224, // SYS_gettid
				231, // SYS_fgetxattr
				232, // SYS_listxattr
				239, // SYS_sendfile64
				240, // SYS_futex
				244, // SYS_get_thread_area
				250, // SYS_fadvise64
				252, // SYS_exit_group
				265, // SYS_clock_gettime
				266, // SYS_clock_getres
				267, // SYS_clock_nanosleep
				269, // SYS_fstatfs64
				272, // SYS_fadvise64_64
				308, // SYS_pselect6
				309, // SYS_ppoll
				312, // SYS_get_robust_list
				323, // SYS_eventfd
				328, // SYS_eventfd2
				333, // SYS_preadv
				334, // SYS_pwritev
				355, // SYS_getrandom
				376, // SYS_mlock2
			}};
			constexpr std::array<int, 64> allowed_syscalls_x86_64 {{
				0, // SYS_read
				1, // SYS_write
				3, // SYS_close
				5, // SYS_fstat
				7, // SYS_poll
				9, // SYS_mmap
				10, // SYS_mprotect
				11, // SYS_munmap
				12, // SYS_brk
				13, // SYS_rt_sigaction
				14, // SYS_rt_sigprocmask
				17, // SYS_pread64
				18, // SYS_pwrite64
				19, // SYS_readv
				20, // SYS_writev
				23, // SYS_select
				25, // SYS_mremap
				26, // SYS_msync
				28, // SYS_madvise
				34, // SYS_pause
				35, // SYS_nanosleep
				37, // SYS_alarm
				39, // SYS_getpid
				40, // SYS_sendfile
				60, // SYS_exit
				73, // SYS_flock
				74, // SYS_fsync
				75, // SYS_fdatasync
				96, // SYS_gettimeofday
				97, // SYS_getrlimit
				98, // SYS_getrusage
				102, // SYS_getuid
				104, // SYS_getgid
				107, // SYS_geteuid
				108, // SYS_getegid
				125, // SYS_capget
				127, // SYS_rt_sigpending
				128, // SYS_rt_sigtimedwait
				130, // SYS_rt_sigsuspend
				138, // SYS_fstatfs
				149, // SYS_mlock
				150, // SYS_munlock
				151, // SYS_mlockall
				152, // SYS_munlockall
				186, // SYS_gettid
				193, // SYS_fgetxattr
				196, // SYS_flistxattr
				201, // SYS_time
				202, // SYS_futex
				211, // SYS_get_thread_area
				221, // SYS_fadvise64
				228, // SYS_clock_gettime
				229, // SYS_clock_getres
				230, // SYS_clock_nanosleep
				231, // SYS_exit_group
				270, // SYS_pselect6
				271, // SYS_ppoll
				274, // SYS_get_robust_list
				284, // SYS_eventfd
				290, // SYS_eventfd2
				295, // SYS_preadv
				296, // SYS_pwritev
				318, // SYS_getrandom
				325, // SYS_mlock2t
			}};

			static_assert(meta::is_sorted(allowed_syscalls_i386), "");
			static_assert(meta::is_sorted(allowed_syscalls_x86_64), "");

			return is_syscall_entry_allowed(pid, syscall, allowed_syscalls_i386,
				allowed_syscalls_x86_64, allowed_files);
		}

		bool is_sysopen(int syscall) const noexcept {
			constexpr int sys_open[2] = {
				5, // SYS_open - i386
				2 // SYS_open - x86_64
			};
			return (syscall == sys_open[arch_]);
		}

		bool is_sysinfo(int syscall) const noexcept {
			constexpr int sys_sysinfo[2] = {
				116, // SYS_sysinfo - i386
				99 // SYS_sysinfo - x86_64
			};
			return (syscall == sys_sysinfo[arch_]);
		}

		bool is_systgkill(int syscall) const noexcept {
			constexpr int sys_tgkill[2] = {
				270, // SYS_tgkill - i386
				234 // SYS_tgkill - x86_64
			};
			return (syscall == sys_tgkill[arch_]);
		}

		bool is_syslseek(int syscall) const noexcept {
			// ARCH_i386
			if (arch_ == ARCH_i386)
				return (syscall == 19 or // SYS_lseek
					syscall == 140); // SYS__llseek

			// ARCH_x86_64
			return (syscall == 8); // SYS_lseek
		}

		bool is_sysdup(int syscall) const noexcept {
			constexpr std::array<int, 3> sys_dup[2] = {
				{{ // i386
					41, // SYS_dup
					63, // SYS_dup2
					330 // SYS_dup3
				}},
				{{ // x86_64
					32, // SYS_dup
					33, // SYS_dup2
					292 // SYS_dup3
				}}
			};
			static_assert(meta::is_sorted(sys_dup[0]), "binsearch below");
			static_assert(meta::is_sorted(sys_dup[1]), "binsearch below");
			return binary_search(sys_dup[arch_], syscall);
		}

	public:
		DefaultCallback() = default;

		virtual ~DefaultCallback() = default;

		bool is_syscall_entry_allowed(pid_t pid, int syscall) {
			return is_syscall_entry_allowed(pid, syscall, {});
		}

		bool is_syscall_exit_allowed(pid_t pid, int syscall);

		std::string error_message() const { return error_message_; }
	};

	using Spawner::ExitStat;
	using Spawner::Options;

	/**
	 * @brief Runs @p exec with arguments @p args with limits @p opts.time_limit
	 *   and @p opts.memory_limit under ptrace(2)
	 * @details Callback object is called on every syscall entry called by exec
	 *   with parameters: child pid, syscall number.
	 *   Callback::operator() must return whether syscall is allowed or not
	 *   @p exec is called via execvp()
	 *   This function is thread-safe.
	 *   IMPORTANT: To function properly this function uses internally signals
	 *     SIGRTMIN and SIGRTMIN + 1 and installs handlers for them. So be aware
	 *     that using these signals while this function runs (in any thread) is
	 *     not safe. Moreover if your program installed handler for the above
	 *     signals, it must install them again after the function returns.
	 *
	 * @param exec filename that is to be executed
	 * @param args arguments passed to exec
	 * @param opts options (new_stdin_fd, new_stdout_fd, new_stderr_fd - file
	 *   descriptors to which respectively stdin, stdout, stderr of sandboxed
	 *   process will be changed or if negative, closed;
	 *   time_limit set to 0 disables the time limit;
	 *   memory_limit set to 0 disables memory limit)
	 * @param working_dir directory at which @p exec will be run
	 * @param func callback functor (used to determine if a syscall should be
	 *   executed)
	 *
	 * @return Returns ExitStat structure with fields:
	 *   - runtime: in timespec structure {sec, nsec}
	 *   - si: {
	 *       code: si_code form siginfo_t from waitid(2)
	 *       status: si_status form siginfo_t from waitid(2)
	 *     }
	 *   - rusage: resource used (see getrusage(2)).
	 *   - vm_peak: peak virtual memory size [bytes]
	 *   - message: detailed info about error, etc.
	 *
	 * @errors Throws an exception std::runtime_error with appropriate
	 *   information if any syscall fails
	 */
	template<class Callback = DefaultCallback>
	static ExitStat run(CStringView exec, const std::vector<std::string>& args,
		const Options& opts = Options(),
		CStringView working_dir = CStringView {"."},
		Callback&& func = Callback());
};

/******************************* IMPLEMENTATION *******************************/

struct Sandbox::i386_user_regset {
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

struct Sandbox::x86_64_user_regset {
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

struct Sandbox::CallbackBase::Registers {
	union user_regs_union {
		i386_user_regset i386_regs;
		x86_64_user_regset x86_64_regs;
	} uregs;

	Registers() = default;

	void get_regs(pid_t pid) {
		struct iovec ivo = {
			&uregs,
			sizeof(uregs)
		};
		if (ptrace(PTRACE_GETREGSET, pid, 1, &ivo) == -1)
			THROW("Error: ptrace(PTRACE_GETREGS)", errmsg());
	}

	void set_regs(pid_t pid) {
		struct iovec ivo = {
			&uregs,
			sizeof(uregs)
		};
		// Update traced process registers
		if (ptrace(PTRACE_SETREGSET, pid, 1, &ivo) == -1)
			THROW("Error: ptrace(PTRACE_SETREGS)", errmsg());
	}
};

struct Sandbox::CallbackBase::SyscallRegisters :
	protected Sandbox::CallbackBase::Registers
{
private:
	std::remove_const_t<decltype(ARCH_x86_64)> arch_ = -1;

public:
	using Registers::uregs;

    SyscallRegisters() = default;

	SyscallRegisters(pid_t pid, decltype(arch_) arch) {
        get_regs(pid, arch);
    }

	void get_regs(pid_t pid, decltype(arch_) arch) {
		Registers::get_regs(pid);
		arch_ = arch;
	}

	using Registers::set_regs;

#define GET_REG(name, reg_name_32, reg_name_64)                                \
    int64_t name ## i() const noexcept {                                       \
        return (arch_ == ARCH_i386 ? (int32_t)uregs.i386_regs.reg_name_32      \
            : uregs.x86_64_regs.reg_name_64);                                  \
    }                                                                          \
                                                                               \
    uint64_t name ## u() const noexcept {                                      \
        return (arch_ == ARCH_i386 ? uregs.i386_regs.reg_name_32               \
            : uregs.x86_64_regs.reg_name_64);                                  \
    }

	GET_REG(res, eax, rax)
	GET_REG(arg1, ebx, rdi)
	GET_REG(arg2, ecx, rsi)
	GET_REG(arg3, edx, rdx)
	GET_REG(arg4, esi, r10)
	GET_REG(arg5, edi, r8)
	GET_REG(arg6, ebp, r9)

#undef GET_REG

#define SET_REG(name, reg_name_32, reg_name_64)                                \
    void set_##name##i(int64_t val) noexcept {                                 \
        if (arch_ == ARCH_i386)                                                \
            uregs.i386_regs.reg_name_32 = (int32_t)val;                        \
        else                                                                   \
            uregs.x86_64_regs.reg_name_64 = val;                               \
    }                                                                          \
                                                                               \
    void set_##name##u(uint64_t val) noexcept {                                \
        if (arch_ == ARCH_i386)                                                \
            uregs.i386_regs.reg_name_32 = val;                                 \
        else                                                                   \
            uregs.x86_64_regs.reg_name_64 = val;                               \
    }

	SET_REG(res, eax, rax)
	SET_REG(arg1, ebx, rdi)
	SET_REG(arg2, ecx, rsi)
	SET_REG(arg3, edx, rdx)
	SET_REG(arg4, esi, r10)
	SET_REG(arg5, edi, r8)
	SET_REG(arg6, ebp, r9)

#undef SET_REG
#undef SET_REG
};

template<size_t N1, size_t N2>
bool Sandbox::DefaultCallback::is_syscall_in(int syscall,
	const std::array<int, N1>& syscall_list_i386,
	const std::array<int, N2>& syscall_list_x86_64) const noexcept
{
	if (arch_ == ARCH_i386)
		return binary_search(syscall_list_i386, syscall);

	return binary_search(syscall_list_x86_64, syscall);
}

template<size_t N1, size_t N2>
bool Sandbox::DefaultCallback::is_syscall_entry_allowed(pid_t pid, int syscall,
	const std::array<int, N1>& allowed_syscalls_i386,
	const std::array<int, N2>& allowed_syscalls_x86_64,
	const std::vector<std::pair<std::string, OpenAccess>>& allowed_files)
{
	// Check if syscall is allowed
	if (is_syscall_in(syscall, allowed_syscalls_i386, allowed_syscalls_x86_64))
		return true;

	// Check if syscall is limited
	for (Pair& i : limited_syscalls[arch_])
		if (syscall == i.syscall)
			return (--i.limit >= 0);

	if (is_sysopen(syscall))
		return is_open_allowed(pid, allowed_files);

	if (is_syslseek(syscall) or is_sysdup(syscall))
		return is_lseek_or_dup_allowed(pid);

	if (is_sysinfo(syscall))
		return is_sysinfo_allowed(pid);

	if (is_systgkill(syscall))
		return is_tgkill_allowed(pid);

	return false;
}

template<class Callback>
Sandbox::ExitStat Sandbox::run(CStringView exec,
	const std::vector<std::string>& args, const Options& opts,
	CStringView working_dir, Callback&& func)
{
	static_assert(std::is_base_of<CallbackBase, Callback>::value,
		"Callback has to derive from Sandbox::CallbackBase");

	// Set up error stream from tracee (and wait_for_syscall()) via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", errmsg());

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", errmsg());

	else if (cpid == 0) { // Child = tracee
		sclose(pfd[0]);
		run_child(exec, args, opts, working_dir, pfd[1], [=]{
			if (ptrace(PTRACE_TRACEME, 0, 0, 0))
				send_error_message(pfd[1], errno, "ptrace(PTRACE_TRACEME)");
		});
	}

	sclose(pfd[1]);
	Closer close_pipe0(pfd[0]); // Guard closing of the pipe second end

	// Wait for tracee to be ready
	siginfo_t si;
	rusage ru;
	waitid(P_PID, cpid, &si, WSTOPPED);
	// If something went wrong
	if (si.si_code != CLD_TRAPPED)
		return ExitStat({0, 0}, {0, 0}, si.si_code, si.si_status, ru, 0,
			receive_error_message(si, pfd[0]));

	// Useful when exception is thrown
	auto kill_and_wait_tracee_guard = make_call_in_destructor([&] {
		kill(-cpid, SIGKILL);
		waitid(P_PID, cpid, &si, WEXITED);
	});

#ifndef PTRACE_O_EXITKILL
# define PTRACE_O_EXITKILL 0
#endif
	// Set up ptrace options
	if (ptrace(PTRACE_SETOPTIONS, cpid, 0, PTRACE_O_TRACESYSGOOD |
		PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL))
	{
		THROW("ptrace(PTRACE_SETOPTIONS)", errmsg());
	}

	func.detect_tracee_architecture(cpid);

	// Open /proc/{cpid}/statm
	FileDescriptor statm_fd {concat("/proc/", cpid, "/statm").to_cstr(),
		O_RDONLY};
	if (statm_fd == -1)
		THROW("open(/proc/{cpid}/statm)", errmsg());

	auto get_vm_size = [&] {
		std::array<char, 32> buff;
		ssize_t rc = pread(statm_fd, buff.data(), buff.size() - 1, 0);
		if (rc <= 0)
			THROW("pread()", errmsg());

		buff[rc] = '\0';

		// Obtain value
		uint64_t vm_size = 0;
		for (int i = 0; i < 32 && isdigit(buff[i]); ++i)
			vm_size = vm_size * 10 + buff[i] - '0';

		DEBUG_SANDBOX(stdlog("get_vm_size: -> ", vm_size);)
		return vm_size;
	};

	uint64_t vm_size = 0; // In pages
	timespec runtime {};
	timespec cpu_time {};

	// Set up timers
	Timer timer(cpid, opts.real_time_limit);
	CPUTimeMonitor cpu_timer(cpid, opts.cpu_time_limit);

	auto get_cpu_time_and_wait_tracee = [&](bool is_waited = false) {
		// Wait cpid so that the CPU runtime will be accurate
		if (not is_waited)
			waitid(P_PID, cpid, &si, WEXITED | WNOWAIT);

		// Get cpu_time
		clockid_t cid;
		if (clock_getcpuclockid(cpid, &cid) == 0)
			clock_gettime(cid, &cpu_time);

		cpu_timer.deactivate();
		kill_and_wait_tracee_guard.cancel(); // Tracee has died
		syscall(SYS_waitid, P_PID, cpid, &si, WEXITED, &ru);
		runtime = timer.stop_and_get_runtime(); // This have to be last because
		                                        // it may throw
	};

	auto wait_for_syscall = [&]() -> int {
		for (;;) {
			(void)ptrace(PTRACE_SYSCALL, cpid, 0, 0); // Fail indicates that
			                                          // the tracee has just
			                                          // died
			waitid(P_PID, cpid, &si, WSTOPPED | WEXITED | WNOWAIT);

			switch (si.si_code) {
			case CLD_TRAPPED:
				if (si.si_status == (SIGTRAP | (PTRACE_EVENT_EXEC<<8)))
					continue; // Ignore exec() events

				if (si.si_status != (SIGTRAP | 0x80)) {
					// Deliver intercepted signal to tracee
					(void)ptrace(PTRACE_CONT, cpid, 0, si.si_status);
					continue;
				}

				return 0;

			case CLD_STOPPED:
					// Deliver intercepted signal to tracee
					(void)ptrace(PTRACE_CONT, cpid, 0, si.si_status);
					break;

			case CLD_EXITED:
			case CLD_DUMPED:
			case CLD_KILLED:
				// Process terminated
				get_cpu_time_and_wait_tracee(true);
				return -1;
			}
		}
	};

	for (;;) {
		auto exited_normally = [&]() -> ExitStat {
			if (si.si_code != CLD_EXITED or si.si_status != 0)
				return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
					vm_size * sysconf(_SC_PAGESIZE),
					receive_error_message(si, pfd[0]));

			return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
				vm_size * sysconf(_SC_PAGESIZE));
		};

		// Syscall entry
		if (wait_for_syscall())
			return exited_normally();

		// Get syscall no.
		errno = 0;
	#ifdef __x86_64__
		long syscall_no = ptrace(PTRACE_PEEKUSER, cpid,
			offsetof(x86_64_user_regset, orig_rax), 0);
	#else
		long syscall_no = ptrace(PTRACE_PEEKUSER, cpid,
			offsetof(i386_user_regset, orig_eax), 0);
	#endif

		try {
			if (syscall_no < 0) {
				if (errno != ESRCH)
					THROW("failed to get syscall_no - ptrace(): ", syscall_no,
						errmsg());

				// Tracee has just died, probably because exceeding the time
				// limit
				kill(-cpid, SIGKILL); // Make sure tracee is (will be) dead
				get_cpu_time_and_wait_tracee(false);

				// Return sandboxing results
				return exited_normally();
			}

			// Some useful debug
			DEBUG_SANDBOX(
				auto syscall_log = stdlog('[', cpid, "] syscall: ",
					paddedString(toStr(syscall_no), 3), ": ",
					// syscall name
					(func.get_arch() == ARCH_i386 ? x86_syscall_name
						: x86_64_syscall_name)[syscall_no]);

				auto log_syscall_args = [&] {
					CallbackBase::SyscallRegisters regs(cpid, func.get_arch());
					syscall_log('(', regs.arg1i(), ", ", regs.arg2i(), ", ...)");
				};

				auto log_syscall_retval = [&] {
					CallbackBase::SyscallRegisters regs(cpid, func.get_arch());
					syscall_log(" -> ", regs.resi());
				};
			)

			// If syscall entry is allowed
			bool allowed = func.is_syscall_entry_allowed(cpid, syscall_no);
			DEBUG_SANDBOX(log_syscall_args();) // Must be placed after the check
				// because the arguments may be altered by it
			if (allowed) {
				// Syscall returns
				if (wait_for_syscall())
					return exited_normally();

				// Monitor for vm_size change
				if (func.get_arch() == ARCH_i386
					? isIn(syscall_no, { // i386
						11, // SYS_execve
						45, // SYS_brk
						90, // SYS_mmap
						163, // SYS_mremap
						192, // SYS_mmap2
					})
					: isIn(syscall_no, { // x86_64
						9, // SYS_mmap
						12, // SYS_brk
						25, // SYS_mremap
						59, // SYS_execve
					}))
				{
					vm_size = std::max(vm_size, get_vm_size());
				}

				// Check syscall after returning
				allowed = func.is_syscall_exit_allowed(cpid, syscall_no);
				DEBUG_SANDBOX(log_syscall_retval();) // Must be placed after the
					// above check because syscall return value is affected by
					// the architecture change, which is detected in the check,
					// and may be affected by the check itself
				if (allowed)
					continue;
			}

		} catch (const std::exception& e) {
			DEBUG_SANDBOX(stdlog(__FILE__ ":", meta::ToString<__LINE__>{},
				": Caught exception: ", e.what());)

			// Exception after tracee is dead and waited
			if (not kill_and_wait_tracee_guard.active())
				throw;

			// Check whether the tracee is still controllable - may not become
			// a zombie in a moment (yes kernel has a minimal delay between
			// these two, moreover waitpid(cpid, ..., WNOHANG) may raturn 0 even
			// though the tracee is not controllable anymore. The process state
			// in /proc/.../stat also may not be `Z`. That is why ptrace(2) is
			// used here)
			errno = 0;
			(void)ptrace(PTRACE_PEEKUSER, cpid, 0, 0);
			// The tracee is no longer controllable
			if (errno == ESRCH) {
				kill(-cpid, SIGKILL); // Make sure tracee is (will be) dead
				get_cpu_time_and_wait_tracee(false);

				// Return sandboxing results
				return exited_normally();
			}

			// Tracee is controllable and other error occured
			throw;
		}

		/* Syscall entry or exit is not allowed */

		// Kill and wait tracee
		kill(-cpid, SIGKILL);
		get_cpu_time_and_wait_tracee(false);

		// Not allowed syscall
		std::string message = func.error_message();
		if (message.empty()) { // func has not left any message
			// Try to get syscall name
			CStringView syscall_name = (func.get_arch() == ARCH_i386 ?
				x86_syscall_name : x86_64_syscall_name)[syscall_no];

			if (syscall_name.empty()) // Syscall not found
				message = concat_tostr("forbidden syscall ", syscall_no);
			else
				message = concat_tostr("forbidden syscall ", syscall_no, ": ",
					syscall_name, "()");
		}

		return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
			vm_size * sysconf(_SC_PAGESIZE), message);
	}
}
