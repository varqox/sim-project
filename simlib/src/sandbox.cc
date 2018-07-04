#include "../include/sandbox.h"

#include <linux/version.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/uio.h>

#if 0
# warning "Before committing disable this debug"
# define DEBUG_SANDBOX(...) __VA_ARGS__
constexpr bool DEBUG_SANDBOX_LOG_PFC_FILTER = false;
#else
# define DEBUG_SANDBOX(...)
#endif

using std::make_unique;
using std::unique_ptr;
using std::vector;

#ifndef SYS_SECCOMP
# define SYS_SECCOMP 1
#endif

static_assert(std::is_same<decltype(SCMP_CMP(0, SCMP_CMP_MASKED_EQ, 0, 0)),
	scmp_arg_cmp>::value, "It is needed for the below wrapper to work");

#undef SCMP_CMP

constexpr static inline scmp_arg_cmp SCMP_CMP(decltype(scmp_arg_cmp::arg) arg,
	decltype(scmp_arg_cmp::op) op, decltype(scmp_arg_cmp::datum_a) datum_a,
	decltype(scmp_arg_cmp::datum_b) datum_b = {})
{
	return (scmp_arg_cmp){arg, op, datum_a, datum_b};
}

template<class... Arg>
static inline void seccomp_rule_add_throw(Arg&&... args) {
	int errnum = seccomp_rule_add(std::forward<Arg>(args)...);
	if (errnum)
		THROW("seccomp_rule_add_throw()", errmsg(-errnum));
}

template<class... Arg>
static inline void seccomp_arch_add_throw(Arg&&... args) {
	int errnum = seccomp_arch_add(std::forward<Arg>(args)...);
	if (errnum)
		THROW("seccomp_arch_add()", errmsg(-errnum));
}

template<class... Arg>
static inline void seccomp_arch_remove_throw(Arg&&... args) {
	int errnum = seccomp_arch_remove(std::forward<Arg>(args)...);
	if (errnum)
		THROW("seccomp_arch_remove()", errmsg(-errnum));
}

template<class... Arg>
static inline void seccomp_attr_set_throw(Arg&&... args) {
	int errnum = seccomp_attr_set(std::forward<Arg>(args)...);
	if (errnum)
		THROW("seccomp_attr_set()", errmsg(-errnum));
}

template<class... Arg>
static inline void seccomp_syscall_priority_throw(Arg&&... args) {
	int errnum = seccomp_syscall_priority(std::forward<Arg>(args)...);
	if (errnum)
		THROW("seccomp_syscall_priority()", errmsg(-errnum));
}


struct x86_user_regset {
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

struct x86_64_user_regset {
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

template<class Regset>
struct Registers {
	Regset uregs;

	Registers() = default;

	Registers(pid_t pid) { get_regs(pid); }

	void get_regs(pid_t pid) {
		struct iovec ivo = {
			&uregs,
			sizeof(uregs)
		};
		if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &ivo) == -1)
			THROW("ptrace(PTRACE_GETREGS)", errmsg());
	}

	void set_regs(pid_t pid) {
		struct iovec ivo = {
			&uregs,
			sizeof(uregs)
		};
		// Update traced process registers
		if (ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &ivo) == -1)
			THROW("ptrace(PTRACE_SETREGS)", errmsg());
	}
};

struct X86SyscallRegisters : Registers<x86_user_regset> {
    using Registers::Registers;

    auto& syscall() noexcept { return uregs.orig_eax; }
    const auto& syscall() const noexcept { return uregs.orig_eax; }

    auto& res() noexcept { return uregs.eax; }
    const auto& res() const noexcept { return uregs.eax; }

    auto& arg1() noexcept { return uregs.ebx; }
    const auto& arg1() const noexcept { return uregs.ebx; }

    auto& arg2() noexcept { return uregs.ecx; }
    const auto& arg2() const noexcept { return uregs.ecx; }

    auto& arg3() noexcept { return uregs.edx; }
    const auto& arg3() const noexcept { return uregs.edx; }

    auto& arg4() noexcept { return uregs.esi; }
    const auto& arg4() const noexcept { return uregs.esi; }

    auto& arg5() noexcept { return uregs.edi; }
    const auto& arg5() const noexcept { return uregs.edi; }

    auto& arg6() noexcept { return uregs.ebp; }
    const auto& arg6() const noexcept { return uregs.ebp; }
};

struct X86_64SyscallRegisters : Registers<x86_64_user_regset> {
    using Registers::Registers;

    auto& syscall() noexcept { return uregs.orig_rax; }
    const auto& syscall() const noexcept { return uregs.orig_rax; }

    auto& res() noexcept { return uregs.rax; }
    const auto& res() const noexcept { return uregs.rax; }

    auto& arg1() noexcept { return uregs.rdi; }
    const auto& arg1() const noexcept { return uregs.rdi; }

    auto& arg2() noexcept { return uregs.rsi; }
    const auto& arg2() const noexcept { return uregs.rsi; }

    auto& arg3() noexcept { return uregs.rdx; }
    const auto& arg3() const noexcept { return uregs.rdx; }

    auto& arg4() noexcept { return uregs.r10; }
    const auto& arg4() const noexcept { return uregs.r10; }

    auto& arg5() noexcept { return uregs.r8; }
    const auto& arg5() const noexcept { return uregs.r8; }

    auto& arg6() noexcept { return uregs.r9; }
    const auto& arg6() const noexcept { return uregs.r9; }
};

template<class Func>
class SyscallCallbackLambda : public SyscallCallback {
	Func func;

public:
	SyscallCallbackLambda(Func&& f) : func(f) {}

	SyscallCallbackLambda(const SyscallCallbackLambda&) = delete;
	SyscallCallbackLambda(SyscallCallbackLambda&&) noexcept = default;
	SyscallCallbackLambda& operator=(const SyscallCallbackLambda&) = delete;
	SyscallCallbackLambda& operator=(SyscallCallbackLambda&&) noexcept = default;

	bool operator()() final {
		return func();
	}
};

#if __cplusplus > 201402L
#warning "Since C++17 calling the constructor is OK"
#endif
template<class Func>
inline unique_ptr<SyscallCallbackLambda<Func>> syscall_callback_lambda(Func&& f) {
	return make_unique<SyscallCallbackLambda<Func>>(std::forward<Func>(f));
}

Sandbox::Sandbox() {
	x86_ctx_ = seccomp_init(SCMP_ACT_TRAP);
	if (not x86_ctx_)
		THROW("seccomp_init()", errmsg());

	x86_64_ctx_ = seccomp_init(SCMP_ACT_TRAP);
	if (not x86_64_ctx_) {
		(void)seccomp_release(x86_ctx_);
		THROW("seccomp_init()", errmsg());
	}

	auto ctx_releaser = [&] {
		(void)seccomp_release(x86_ctx_);
		(void)seccomp_release(x86_64_ctx_);
	};
	CallInDtor<decltype(ctx_releaser)> ctx_releaser_guard {ctx_releaser};

	// Returns the callback id that can be used in the SCMP_ACT_TRACE()
	auto add_unique_ptr_callback = [&](auto&& callback) {
		auto id = callbacks_.size();
		callbacks_.emplace_back(std::forward<decltype(callback)>(callback));
		return id;
	};

	// Returns the callback id that can be used in the SCMP_ACT_TRACE()
	auto add_callback = [&](auto&& callback) {
		return add_unique_ptr_callback(syscall_callback_lambda(
			std::forward<decltype(callback)>(callback)));
	};

	// Enable synchronization (it will not matter if the process has one thread,
	// but it may have more in the future)
	seccomp_attr_set_throw(x86_ctx_, SCMP_FLTATR_CTL_TSYNC, 1);
	seccomp_attr_set_throw(x86_64_ctx_, SCMP_FLTATR_CTL_TSYNC, 1);

	// Set the proper architectures
	if (seccomp_arch_native() != SCMP_ARCH_X86) {
		seccomp_arch_add_throw(x86_ctx_, SCMP_ARCH_X86);
		seccomp_arch_remove_throw(x86_ctx_, SCMP_ARCH_NATIVE);
	}
	if (seccomp_arch_native() != SCMP_ARCH_X86_64) {
		seccomp_arch_add_throw(x86_64_ctx_, SCMP_ARCH_X86_64);
		seccomp_arch_remove_throw(x86_64_ctx_, SCMP_ARCH_NATIVE);
	}

	{
		auto invalid_arch_callback_id = add_callback([&] {
			return set_message_callback("Invalid architecture of the syscall");
		});
		seccomp_attr_set_throw(x86_ctx_, SCMP_FLTATR_ACT_BADARCH,
			SCMP_ACT_TRACE(invalid_arch_callback_id));
		seccomp_attr_set_throw(x86_64_ctx_, SCMP_FLTATR_ACT_BADARCH,
			SCMP_ACT_TRACE(invalid_arch_callback_id));
	}

	/* ======================== Set priorities ======================== */

	{
		auto seccomp_syscall_priority_both_ctx = [&](auto&&... args) {
			seccomp_syscall_priority_throw(x86_ctx_, args...);
			seccomp_syscall_priority_throw(x86_64_ctx_, args...);
		};

		seccomp_syscall_priority_both_ctx(SCMP_SYS(brk), 255);
		seccomp_syscall_priority_both_ctx(SCMP_SYS(mmap2), 255);

		seccomp_syscall_priority_both_ctx(SCMP_SYS(read), 254);
		seccomp_syscall_priority_both_ctx(SCMP_SYS(write), 254);

		seccomp_syscall_priority_both_ctx(SCMP_SYS(mmap), 253);
		seccomp_syscall_priority_both_ctx(SCMP_SYS(mremap), 253);
	}

	/* ======================== Installing callbacks ======================== */
	class SyscallCallbackLimiting : public SyscallCallback {
		Sandbox& sandbox_;
		const uint limit_;
		uint counter_;
		const char* syscall_name_;

	public:
		SyscallCallbackLimiting(Sandbox& sandbox, uint limit, const char* syscall_name)
			: sandbox_(sandbox), limit_(limit), counter_(limit), syscall_name_(syscall_name) {}

		void reset() noexcept {
			counter_ = limit_;
		}

		bool operator()() {
			if (counter_ == 0)
				return sandbox_.set_message_callback("forbidden syscall: ", syscall_name_, "()");

			--counter_;
			return false;
		}
	};

	auto add_limiting_callback = [&](uint limit, const char* syscall_name) {
		auto id = callbacks_.size();
		callbacks_.emplace_back(make_unique<SyscallCallbackLimiting>(*this, limit, syscall_name));
		return id;
	};

	// execve()
	seccomp_rule_add_both_ctx(
		SCMP_ACT_TRACE(add_limiting_callback(1, "execve")), SCMP_SYS(execve),
		0);

	// uname()
	seccomp_rule_add_both_ctx(
		SCMP_ACT_TRACE(add_limiting_callback(1, "uname")), SCMP_SYS(uname), 0);

	// set_thread_area()
	seccomp_rule_add_both_ctx(
		SCMP_ACT_TRACE(add_limiting_callback(1, "set_thread_area")),
		SCMP_SYS(set_thread_area), 0);

	// arch_prctl()
	seccomp_rule_add_throw(x86_64_ctx_,
		SCMP_ACT_TRACE(add_limiting_callback(1, "arch_prctl")),
		SCMP_SYS(arch_prctl), 0);

	// prlimit64() - needed by callback to Sandbox::run_child() to limit the VM
	// size
	seccomp_rule_add_both_ctx(
		SCMP_ACT_TRACE(add_limiting_callback(2, "prlimit64")),
		SCMP_SYS(prlimit64), 0);

	// access
	seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(ENOENT), SCMP_SYS(access), 0);

	// readlink
	seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(ENOENT), SCMP_SYS(readlink), 0);

	// Monitor memory for changes of tracee's virtual memory size
	{
		/* This is needed to prevent tracee going over the time limit when it is
		 * flooding the kernel with unsuccessful syscalls that ask for more
		 * memory. No normal program would constantly unsuccessfully ask for
		 * memory. Because of that, instead of timeout you will get
		 * "Memory limit exceeded". All this is done to improve the readability
		 * of ExitStatus.
		 */
		constexpr uint FAIL_COUNTER_LIMIT = 1024; // No sane process would do so many subsequent unsuccessful allocations
		class SyscallCallbackVmPeakUpdater : public SyscallCallback {
			Sandbox& sandbox_;
			uint fail_counter_ = 0;

		public:
			SyscallCallbackVmPeakUpdater(Sandbox& sandbox) : sandbox_(sandbox) {}

			void reset() noexcept {
				fail_counter_ = 0;
			}

			bool operator()() {
				// Advance to the syscall exit
				(void)ptrace(PTRACE_SYSCALL, sandbox_.tracee_pid_, 0, 0);
				siginfo_t si;
				waitid(P_PID, sandbox_.tracee_pid_, &si, WSTOPPED | WEXITED | WNOWAIT);
				if (si.si_status != (SIGTRAP | 0x80)) // Something other e.g. a signal (ignore)
					return false; // Allow the signal to get to the tracee

				auto vm_size = sandbox_.get_tracee_vm_size();
				if (vm_size == sandbox_.tracee_vm_peak_) {
					if (++fail_counter_ == FAIL_COUNTER_LIMIT)
						return sandbox_.set_message_callback("Memory limit exceeded");

				} else {
					fail_counter_ = 0;
					if (vm_size > sandbox_.tracee_vm_peak_)
						sandbox_.tracee_vm_peak_ = vm_size;
				}

				DEBUG_SANDBOX(stdlog("tracee_vm_peak: -> ", sandbox_.tracee_vm_peak_);)
				return false;
			}
		};

		auto update_vm_peak_callback_id =
			add_unique_ptr_callback(make_unique<SyscallCallbackVmPeakUpdater>(*this));
		seccomp_rule_add_both_ctx(SCMP_ACT_TRACE(update_vm_peak_callback_id), SCMP_SYS(brk), 0);
		seccomp_rule_add_both_ctx(SCMP_ACT_TRACE(update_vm_peak_callback_id), SCMP_SYS(mmap), 0);
		seccomp_rule_add_both_ctx(SCMP_ACT_TRACE(update_vm_peak_callback_id), SCMP_SYS(mmap2), 0);
		seccomp_rule_add_both_ctx(SCMP_ACT_TRACE(update_vm_peak_callback_id), SCMP_SYS(mremap), 0);
		// Needed here to reset the fail_counter_
		seccomp_rule_add_both_ctx(SCMP_ACT_TRACE(update_vm_peak_callback_id), SCMP_SYS(munmap), 0);
	}

	// Allow only allowed_files_ to be opened
	{
		static auto check_opening_syscall = [](pid_t tracee_pid, auto& registers,
			auto& syscall_reg, auto& res_reg, auto path_arg, auto flags_arg,
			const vector<AllowedFile>& allowed_files)
		{
			// This (getting the filename) was tested against malicious pointers
			// passed to open near the page boundary and it worked well
			struct iovec local, remote;
			char buff[PATH_MAX + 1] = {};
			local.iov_base = buff;
			remote.iov_base = reinterpret_cast<void*>((size_t)path_arg);
			local.iov_len = remote.iov_len = PATH_MAX;

			ssize_t len = process_vm_readv(tracee_pid, &local, 1, &remote, 1, 0);
			if (len < 0) {
				syscall_reg = -1;
				res_reg = -EFAULT;
				registers.set_regs(tracee_pid);
				return false;
			}

			StringView path(buff, len);
			path = path.extractPrefix(path.find('\0'));
			DEBUG_SANDBOX(auto tmplog = stdlog("Trying to open: '", path, '\'');)
			if (path.size() == (size_t)len) {
				syscall_reg = -1;
				res_reg = -ENAMETOOLONG;
				registers.set_regs(tracee_pid);
				DEBUG_SANDBOX(tmplog(" - disallowed");)
				return false;
			}

			// TODO: maybe sort the allowed files?
			static_assert(O_RDONLY == 0, "Needed below");
			static_assert(O_WRONLY == 1, "Needed below");
			static_assert(O_RDWR == 2, "Needed below");
			static_assert(O_ACCMODE == 3, "Needed below");

			auto perm = flags_arg & O_ACCMODE;

			OpenAccess op = OpenAccess::NONE;
			if (perm == O_RDONLY)
				op = OpenAccess::RDONLY;
			else if (perm == O_WRONLY)
				op = OpenAccess::WRONLY;
			else if (perm == O_RDWR)
				op = OpenAccess::RDWR;

			DEBUG_SANDBOX(
				switch (op) {
				case OpenAccess::NONE: tmplog(" NONE"); break;
				case OpenAccess::RDONLY: tmplog(" RDONLY"); break;
				case OpenAccess::WRONLY: tmplog(" WRONLY"); break;
				case OpenAccess::RDWR: tmplog(" RDWR"); break;
				}
			)

			for (AllowedFile const& af : allowed_files)
				if (af.first == path) {
					if (op == af.second) {
						DEBUG_SANDBOX(tmplog(" - ok");)
						return false; // Allow to open
					}

					break; // Invalid opening mode -> disallow
				}

			// File is not allowed to be opened
			syscall_reg = -1;
			res_reg = -EPERM;
			registers.set_regs(tracee_pid);
			DEBUG_SANDBOX(tmplog(" - disallowed");)

			return false;
		};

		static auto check_openat_syscall = [](pid_t tracee_pid, auto& regs,
			const vector<AllowedFile>& allowed_files)
		{
			// Currently opening at directory fd other than AT_FDCWD is not allowed
			if ((int)regs.arg1() != AT_FDCWD) {
				DEBUG_SANDBOX(stdlog("Trying to openat at: ", regs.arg1(),
					" - disallowed");)

				regs.syscall() = -1;
				regs.res() = -EPERM;
				regs.set_regs(tracee_pid);
				return false;
			}

			return check_opening_syscall(tracee_pid, regs, regs.syscall(),
				regs.res(), regs.arg2(), regs.arg3(), allowed_files);
		};

		// open() - x86
		seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_TRACE(add_callback([&]{
			X86SyscallRegisters regs(tracee_pid_);
			return check_opening_syscall(tracee_pid_, regs, regs.syscall(),
				regs.res(), regs.arg1(), regs.arg2(), *allowed_files_);
		})), SCMP_SYS(open), 0);

		// open() - x86_64
		seccomp_rule_add_throw(x86_64_ctx_, SCMP_ACT_TRACE(add_callback([&]{
			X86_64SyscallRegisters regs(tracee_pid_);
			return check_opening_syscall(tracee_pid_, regs, regs.syscall(),
				regs.res(), regs.arg1(), regs.arg2(), *allowed_files_);
		})), SCMP_SYS(open), 0);

		// openat() - x86
		seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_TRACE(add_callback([&]{
			X86SyscallRegisters regs(tracee_pid_);
			return check_openat_syscall(tracee_pid_, regs, *allowed_files_);
		})), SCMP_SYS(openat), 0);

		// openat() - x86_64
		seccomp_rule_add_throw(x86_64_ctx_, SCMP_ACT_TRACE(add_callback([&]{
			X86_64SyscallRegisters regs(tracee_pid_);
			return check_openat_syscall(tracee_pid_, regs, *allowed_files_);
		})), SCMP_SYS(openat), 0);
	}

	// sysinfo
	seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(sysinfo), 0);

	// ioctl
	seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ioctl), 0);

	static_assert(STDIN_FILENO == 0, "Needed below");
	static_assert(STDOUT_FILENO == 1, "Needed below");
	static_assert(STDERR_FILENO == 2, "Needed below");
#define DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(arch_ctx, syscall, errno) \
	seccomp_rule_add_throw(arch_ctx, SCMP_ACT_ERRNO(errno), SCMP_SYS(syscall), 1, \
		SCMP_A0(SCMP_CMP_EQ, STDIN_FILENO)); \
	seccomp_rule_add_throw(arch_ctx, SCMP_ACT_ERRNO(errno), SCMP_SYS(syscall), 1, \
		SCMP_A0(SCMP_CMP_EQ, STDOUT_FILENO)); \
	seccomp_rule_add_throw(arch_ctx, SCMP_ACT_ERRNO(errno), SCMP_SYS(syscall), 1, \
		SCMP_A0(SCMP_CMP_EQ, STDERR_FILENO)); \
\
	seccomp_rule_add_throw(arch_ctx, SCMP_ACT_ALLOW, SCMP_SYS(syscall), 1, \
		SCMP_A0(SCMP_CMP_LT, STDIN_FILENO)); \
	seccomp_rule_add_throw(arch_ctx, SCMP_ACT_ALLOW, SCMP_SYS(syscall), 1, \
		SCMP_A0(SCMP_CMP_GT, STDERR_FILENO));

#define DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(syscall, errno) \
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(x86_ctx_, syscall, errno); \
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(x86_64_ctx_, syscall, errno)

	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(x86_ctx_, _llseek, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(x86_ctx_, fadvise64_64, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR(x86_ctx_, fstat64, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(dup, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(dup2, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(dup3, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(fadvise64, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(flistxattr, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(flock, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(fstat, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(fsync, EPERM);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(lseek, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(pread64, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(preadv, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(pwrite64, ESPIPE);
	DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX(pwritev, ESPIPE);

#undef DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR
#undef DISALLOW_ONLY_ON_STDIN_STDOUT_STDERR_BOTH_CTX

	// Allowed syscalls (both architectures)
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(alarm), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(capget), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(capget), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(clock_getres), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(get_robust_list), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(get_thread_area), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getrusage), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(gettid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(mlock), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(mlock2), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(mlockall), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(msync), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(munlock), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(munlockall), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(nanosleep), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(pause), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(poll), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(pselect6), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(readv), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(rt_sigpending), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(rt_sigsuspend), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(rt_sigtimedwait), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(select), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(sendfile), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(time), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
	seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(writev), 0);
	// Allowed syscalls (x86 architecture)
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(_newselect), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(getegid32), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(geteuid32), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(getgid32), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(getuid32), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(sendfile64), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(sigaction), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(sigpending), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(sigsuspend), 0);
	seccomp_rule_add_throw(x86_ctx_, SCMP_ACT_ALLOW, SCMP_SYS(ugetrlimit), 0);
	// Allowed syscalls (x86_64 architecture)
	// -------------------

	ctx_releaser_guard.cancel();
}

template<class... T>
inline void Sandbox::seccomp_rule_add_both_ctx(T&&... args) {
	seccomp_rule_add_throw(x86_ctx_, args...);
	seccomp_rule_add_throw(x86_64_ctx_, args...);
}

template<class... T>
inline bool Sandbox::set_message_callback(T&&... args) {
	message_to_set_in_exit_stat_ = concat_tostr(std::forward<T>(args)...);
	return true;
}

inline void Sandbox::reset_callbacks() noexcept {
	for (auto& p : callbacks_)
		p.get()->reset();
}

uint64_t Sandbox::get_tracee_vm_size() {
	std::array<char, 32> buff;
	ssize_t rc = pread(tracee_statm_fd_, buff.data(), buff.size() - 1, 0);
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

Sandbox::ExitStat Sandbox::run(CStringView exec,
	const std::vector<std::string>& exec_args, const Options& opts,
	const std::vector<AllowedFile>& allowed_files)
{
	// Reset the state
	reset_callbacks();
	tracee_vm_peak_ = 0;
	message_to_set_in_exit_stat_.clear();
	allowed_files_ = &allowed_files;

	// Set up error stream from tracee (and wait_for_syscall()) via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", errmsg());

	tracee_pid_ = fork();
	if (tracee_pid_ == -1)
		THROW("fork()", errmsg());

	else if (tracee_pid_ == 0) { // Child = tracee
		sclose(pfd[0]);

		auto send_error_and_exit = [&](auto&&... args) {
			send_error_message_and_exit(pfd[1],
				std::forward<decltype(args)>(args)...);
		};

		DEBUG_SANDBOX(
			int stdlog_fd_copy = -1;
			if (DEBUG_SANDBOX_LOG_PFC_FILTER) {
				stdlog_fd_copy = fcntl(stdlog.fileno(), F_DUPFD_CLOEXEC);
				if (stdlog_fd_copy == -1)
					send_error_and_exit(errno, "fcntl(F_DUPFD_CLOEXEC)");
			}
		)

		// Memory limit will be set manually after loading the filters as
		// seccomp_load() needs to allocate more memory and it may fail if this
		// memory limit is very restrictive, which is not what we want
		Options run_child_opts = opts;
		run_child_opts.memory_limit = 0;

		run_child(exec, exec_args, run_child_opts, pfd[1], [=]{
			tracee_pid_ = getpid();
			/* =============== Rules depending on tracee_pid_ =============== */
			try {
				// tgkill (allow only killing the calling process / thread)
				seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(tgkill), 2,
					SCMP_A0(SCMP_CMP_EQ, tracee_pid_),
					SCMP_A1(SCMP_CMP_EQ, tracee_pid_));
				seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(tgkill), 1,
					SCMP_A0(SCMP_CMP_NE, tracee_pid_));
				seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(tgkill), 2,
					SCMP_A0(SCMP_CMP_EQ, tracee_pid_),
					SCMP_A1(SCMP_CMP_NE, tracee_pid_));
				// kill (allow only killing the calling process)
				seccomp_rule_add_both_ctx(SCMP_ACT_ALLOW, SCMP_SYS(kill), 1,
					SCMP_A0(SCMP_CMP_EQ, tracee_pid_));
				seccomp_rule_add_both_ctx(SCMP_ACT_ERRNO(EPERM), SCMP_SYS(kill), 1,
					SCMP_A0(SCMP_CMP_NE, tracee_pid_));

			} catch (const std::exception& e) {
				send_error_and_exit(CStringView(e.what()));
			}

			// Merge filters for both architectures into one filter
			int errnum = seccomp_merge(x86_ctx_, x86_64_ctx_);
			if (errnum)
				send_error_and_exit(-errnum, "seccomp_merge()");

			auto& ctx = x86_ctx_;

			// Dump filter rules
			DEBUG_SANDBOX({
				if (DEBUG_SANDBOX_LOG_PFC_FILTER) {
					errnum = seccomp_export_pfc(ctx, stdlog_fd_copy);
					if (errnum)
						send_error_and_exit(-errnum, "seccomp_export_pfc()");
				}
			})

			// Set max core dump size to 0 in order to avoid creating redundant
			// core dumps
			{
				struct rlimit limit;
				limit.rlim_max = limit.rlim_cur = 0;
				if (setrlimit(RLIMIT_CORE, &limit))
					send_error_and_exit(errno, "setrlimit(RLIMIT_CORE)");
			}

			if (ptrace(PTRACE_TRACEME, 0, 0, 0))
				send_error_and_exit(errno, "ptrace(PTRACE_TRACEME)");

			// Signal the tracer that ptrace is ready and it may proceed to
			// tracing us. It has to be done before loading the filter into the
			// kernel because if some allocation occurs during the
			// seccomp_load() but after loading the filter then it will fail
			// (because of seccomp rules defined for memory allocations) and
			// probably fail this process - it happened under Address Sanitizer,
			// so it's better to take precautions.
			kill(getpid(), SIGSTOP);

			// Load filter into the kernel
			errnum = seccomp_load(ctx);
			if (errnum)
				send_error_and_exit(-errnum, "seccomp_load()");

			// Set virtual memory and stack size limit (to the same value)
			if (opts.memory_limit > 0) {
				struct rlimit limit;
				limit.rlim_max = limit.rlim_cur = opts.memory_limit;
				if (prlimit(getpid(), RLIMIT_AS, &limit, nullptr))
					send_error_and_exit(errno, "prlimit(RLIMIT_AS)");
				if (prlimit(getpid(), RLIMIT_STACK, &limit, nullptr))
					send_error_and_exit(errno, "prlimit(RLIMIT_STACK)");

			// Exhaust the limit of calling prlimit64(2) syscall so that the
			// sandboxed process cannot call it
			} else {
				(void)prlimit(getpid(), RLIMIT_AS, nullptr, nullptr);
				(void)prlimit(getpid(), RLIMIT_STACK, nullptr, nullptr);
			}

			// The following SIGSTOP issued by run_child() will be ignored
		});
	}

	sclose(pfd[1]);
	FileDescriptorCloser close_pipe0(pfd[0]); // Guard closing of the pipe's second end

	// Wait for tracee to be ready
	siginfo_t si;
	rusage ru;
	if (waitid(P_PID, tracee_pid_, &si, WSTOPPED | WEXITED) == -1)
		THROW("waitid()", errmsg());

	DEBUG_SANDBOX(stdlog("waitid(): code: ", si.si_code, " status: ",
		si.si_status, " pid: ", si.si_pid, " errno: ", si.si_errno,
		" signo: ", si.si_signo, " syscall: ", si.si_syscall, " arch: ",
		si.si_arch);)

	// If something went wrong
	if (si.si_code != CLD_TRAPPED)
		return ExitStat({0, 0}, {0, 0}, si.si_code, si.si_status, ru, 0,
			receive_error_message(si, pfd[0]));

	// Useful when exception is thrown
	auto kill_and_wait_tracee_guard = make_call_in_destructor([&] {
		kill(-tracee_pid_, SIGKILL);
		waitid(P_PID, tracee_pid_, &si, WEXITED);
	});

	// Set up ptrace options
	if (ptrace(PTRACE_SETOPTIONS, tracee_pid_, 0, PTRACE_O_TRACESYSGOOD |
		PTRACE_O_TRACEEXEC | PTRACE_O_TRACESECCOMP | PTRACE_O_EXITKILL))
	{
		THROW("ptrace(PTRACE_SETOPTIONS)", errmsg());
	}

	// Open /proc/{tracee_pid_}/statm for tracking vm_peak (vm stands for virtual memory)
	tracee_statm_fd_.open(concat("/proc/", tracee_pid_, "/statm").to_cstr(), O_RDONLY);
	if (tracee_statm_fd_ == -1)
		THROW("open(/proc/{tracee_pid_ = ", tracee_pid_, "}/statm)", errmsg());

	auto tracee_statm_fd_guard = make_call_in_destructor([&] {
		tracee_statm_fd_.close();
	});

	timespec runtime {};
	timespec cpu_time {};

	// Set up timers
	unique_ptr<Timer> timer;
	unique_ptr<CPUTimeMonitor> cpu_timer;

	auto get_cpu_time_and_wait_tracee = [&](bool is_waited = false) {
		// Wait tracee_pid_ so that the CPU runtime will be accurate
		if (not is_waited)
			waitid(P_PID, tracee_pid_, &si, WEXITED | WNOWAIT);

		// Get cpu_time
		if (cpu_timer) {
			cpu_timer->deactivate();
			cpu_time = cpu_timer->get_cpu_runtime();
		} else { // The child did not execve() or the execve() failed
			cpu_time = {0, 0};
			tracee_vm_peak_ = 0; // It might contain the vm size from before execve()
		}

		kill_and_wait_tracee_guard.cancel(); // Tracee has died
		syscall(SYS_waitid, P_PID, tracee_pid_, &si, WEXITED, &ru);
		// This have to be the last because it may throw
		runtime = (timer ? timer->stop_and_get_runtime() : timespec{0, 0});
	};

	static_assert(LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0),
		"Needed by the seccomp(2) features used below");

	try {
		for (;;) {
			// The last arg is the signal number to deliver
			(void)ptrace(PTRACE_CONT, tracee_pid_, 0, 0);
			// Waiting for events
			waitid(P_PID, tracee_pid_, &si, WSTOPPED | WEXITED | WNOWAIT);

			DEBUG_SANDBOX(stdlog("waitid(): code: ", si.si_code, " status: ",
				si.si_status, " pid: ", si.si_pid, " errno: ", si.si_errno,
				" signo: ", si.si_signo, " syscall: ", si.si_syscall, " arch: ",
				si.si_arch));

			switch (si.si_code) {
			case CLD_TRAPPED:
				if (si.si_status == (SIGTRAP | (PTRACE_EVENT_EXEC<<8))) {
					tracee_vm_peak_ = get_tracee_vm_size();
					DEBUG_SANDBOX(stdlog("TRAPPED (exec())"));
					// Fire timers
					timer = make_unique<Timer>(tracee_pid_, opts.real_time_limit);
					cpu_timer = make_unique<CPUTimeMonitor>(tracee_pid_, opts.cpu_time_limit);

					continue; // Nothing more to do for the exec() event
				}

				if (si.si_status == SIGSYS) {
					DEBUG_SANDBOX(stdlog("TRAPPED (SIGSYS)"));
					siginfo_t sii;
					if (ptrace(PTRACE_GETSIGINFO, tracee_pid_, 0, &sii))
						THROW("ptrace(PTRACE_GETSIGINFO)", errmsg());

					DEBUG_SANDBOX(stdlog("waitid(): code: ", sii.si_code, " status: ",
						sii.si_status, " pid: ", sii.si_pid, " errno: ", sii.si_errno,
						" signo: ", sii.si_signo, " syscall: ", sii.si_syscall, " arch: ",
						sii.si_arch));

					if (sii.si_code == SYS_SECCOMP) {
						unique_ptr<char, decltype(free)*> syscall_name {
							seccomp_syscall_resolve_num_arch(sii.si_arch, sii.si_syscall),
							free
						};

						if (syscall_name) {
							set_message_callback("forbidden syscall: ",
								sii.si_syscall, " - ", syscall_name.get());
						} else {
							set_message_callback("forbidden syscall: ",
								sii.si_syscall, " (arch: ", sii.si_arch, ')');
						}

						kill(-tracee_pid_, SIGKILL);
						continue; // That is the SIGSYS from seccomp, others SIGSYSes are not what we want
					}
				}

				if (si.si_status == (SIGTRAP | (PTRACE_EVENT_SECCOMP<<8))) {
					DEBUG_SANDBOX(stdlog("TRAPPED (SECCOMP_EVENT)"));
					unsigned long msg;
					if (ptrace(PTRACE_GETEVENTMSG, tracee_pid_, 0, &msg))
						THROW("ptrace(PTRACE_GETEVENTMSG)", errmsg());

					DEBUG_SANDBOX(stdlog("callback id: ", msg));
					if (callbacks_[msg].get()->operator()())
						kill(-tracee_pid_, SIGKILL);

					continue;
				}

				if (si.si_status == (SIGTRAP | 0x80)) { // Should not happen
					DEBUG_SANDBOX(stdlog("TRAPPED (PTRACE_SYSCALL)"));
					continue;
				}

				DEBUG_SANDBOX(stdlog("TRAPPED (unknown) - si_status: ", si.si_status));
				// Deliver intercepted signal to tracee
				(void)ptrace(PTRACE_CONT, tracee_pid_, 0, si.si_status);
				continue;

			case CLD_STOPPED:
				DEBUG_SANDBOX(stdlog("STOPPED - si_status: ", si.si_status));
				// Deliver intercepted signal to tracee
				(void)ptrace(PTRACE_CONT, tracee_pid_, 0, si.si_status);
				continue;

			case CLD_EXITED:
			case CLD_DUMPED:
			case CLD_KILLED:
				DEBUG_SANDBOX(stdlog("ENDED"));
				// Process terminated
				get_cpu_time_and_wait_tracee(true);
				goto tracee_died;
			}
		}

	// Catch exceptions that occur in the middle of doing something. This may
	// happen when the tracee is killed (e.g. by timeout) while we are doing
	// something (e.g. inspecting syscall arguments).
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
		(void)ptrace(PTRACE_PEEKUSER, tracee_pid_, 0, 0);
		// The tracee is no longer controllable
		if (errno == ESRCH) {
			kill(-tracee_pid_, SIGKILL); // Make sure tracee is (will be) dead
			get_cpu_time_and_wait_tracee(false);

			// Return sandboxing results
			goto tracee_died;
		}

		// Tracee is controllable and other kind of error has occurred
		throw;
	}

tracee_died:
	// Message was set
	if (not message_to_set_in_exit_stat_.empty()) {
		return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
			tracee_vm_peak_ * sysconf(_SC_PAGESIZE), message_to_set_in_exit_stat_);
	}

	// Excited abnormally - probably killed by some signal
	if (si.si_code != CLD_EXITED or si.si_status != 0) {
		return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
			tracee_vm_peak_ * sysconf(_SC_PAGESIZE),
			receive_error_message(si, pfd[0]));
	}

	// Exited normally (maybe with some code != 0)
	return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru,
		tracee_vm_peak_ * sysconf(_SC_PAGESIZE));
}
