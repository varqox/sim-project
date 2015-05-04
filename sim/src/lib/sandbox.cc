#include "../include/debug.h"
#include "../include/memory.h"
#include "../include/sandbox.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::string;
using std::vector;

namespace sandbox {

DefaultCallback::DefaultCallback() : functor_call(0), architecture(-1) {
	// i386
	append(limited_syscalls[0])
		((Pair){  11, 1 }) // SYS_execve
		((Pair){  33, 1 }) // SYS_access
		((Pair){  85, 1 }) // SYS_readlink
		((Pair){ 122, 1 }) // SYS_uname
		((Pair){ 243, 1 }); // SYS_set_thread_area

	append(allowed_syscalls[0])
		(1) // SYS_exit
		(3) // SYS_read
		(4) // SYS_write
		(6) // SYS_close
		(45) // SYS_brk
		(54) // SYS_ioctl
		(90) // SYS_mmap
		(91) // SYS_munmap
		(108) // SYS_fstat
		(192) // SYS_mmap2
		(197) // SYS_fstat64
		(252); // SYS_exit_group

	// x86_64
	append(limited_syscalls[1])
		((Pair){  21, 1 }) // SYS_access
		((Pair){  59, 1 }) // SYS_execve
		((Pair){  63, 1 }) // SYS_uname
		((Pair){  89, 1 }) // SYS_readlink
		((Pair){ 205, 1 }) // SYS_set_thread_area
		((Pair){ 158, 1 }); // SYS_arch_prctl

	append(allowed_syscalls[1])
		(0) // SYS_read
		(1) // SYS_write
		(3) // SYS_close
		(5) // SYS_fstat
		(9) // SYS_mmap
		(11) // SYS_munmap
		(12) // SYS_brk
		(16) // SYS_ioctl
		(60) // SYS_exit
		(231); // SYS_exit_group
};

int DefaultCallback::operator()(int pid, int syscall) {
	// Detect architecture (first call - before exec, second - after exec)
	if (++functor_call < 3) {
		string filename = "/proc/" + toString((long long)pid) + "/exe";

		int fd = open(filename.c_str(), O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			architecture = 0;
			E("Error: '%s' - %s\n", filename.c_str(), strerror(errno));
		} else {
			// Read fourth byte and detect if 32 or 64 bit
			unsigned char c;
			lseek(fd, 4, SEEK_SET);

			int ret = read(fd, &c, 1);
			if (ret == 1 && c == 2)
				architecture = 1; // x86_64
			else
				architecture = 0; // i386

			close(fd);
		}
	}

	// Check if syscall is allowed
	foreach (i, allowed_syscalls[architecture])
		if (syscall == *i)
			return 0;

	// Check if syscall is limited
	foreach (i, limited_syscalls[architecture])
		if (syscall == i->syscall)
			return --i->limit < 0;

	return 1;
}

static pid_t cpid;

static int wait_for_syscall(int* status) {
	for (;;) {
		ptrace(PTRACE_SYSCALL, cpid, 0, 0);
		waitpid(cpid, status, 0);

		if (WIFSTOPPED(*status) && WSTOPSIG(*status) == (SIGTRAP | 0x80))
			return 0;

		if (WIFEXITED(*status) || WIFSIGNALED(*status))
			return -1;
	}
}

static void handle_timeout(int) {
	kill(cpid, SIGKILL);
}

ExitStat run(const string& exec, vector<string> args,
		const struct options *opts, int (*func)(int, int, void*), void *data) {

	cpid = fork();
	if (cpid == -1)
		return ExitStat(-1, 0, string("Failed to fork() - ") + strerror(errno));

	else if (cpid == 0) {
		// Convert args
		const size_t len = args.size();
		char *arg[len + 1];
		arg[len] = NULL;

		for (size_t i = 0; i < len; ++i)
			arg[i] = const_cast<char*>(args[i].c_str());

		// Set virtual memory limit
		if (opts->memory_limit > 0) {
			struct rlimit limit;
			limit.rlim_max = limit.rlim_cur = opts->memory_limit;
			prlimit(getpid(), RLIMIT_AS, &limit, NULL);
		}

		// Change stdin
		if (opts->new_stdin) {
			dup2(fileno(opts->new_stdin), STDIN_FILENO);
			fclose(opts->new_stdin);
		}
		// Change stdout
		if (opts->new_stdout) {
			dup2(fileno(opts->new_stdout), STDOUT_FILENO);
			fclose(opts->new_stdout);
		}
		// Change stderr
		if (opts->new_stderr) {
			dup2(fileno(opts->new_stderr), STDERR_FILENO);
			fclose(opts->new_stderr);
		}

		ptrace(PTRACE_TRACEME);
		kill(getpid(), SIGSTOP);

		execv(exec.c_str(), arg);
		_exit(-1);
	}

	int status;
	waitpid(cpid, &status, 0);
	ptrace(PTRACE_SETOPTIONS, cpid, 0, PTRACE_O_TRACESYSGOOD |
		PTRACE_O_EXITKILL);

	// Set up timer
	// Set handler
	struct sigaction sa, sa_old;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGALRM, &sa, &sa_old);

	// Set timer
	struct itimerval timer, old_timer;
	memset(&timer, 0, sizeof(timer));

	timer.it_value.tv_sec = opts->time_limit / 1000000;
	timer.it_value.tv_usec = opts->time_limit - timer.it_value.tv_sec * 1000000;

	// Run timer (time limit)
	struct timeval tbeg, tend;
	unsigned long long runtime;
	gettimeofday(&tbeg, NULL); // Get start time
	setitimer(ITIMER_REAL, &timer, &old_timer);

	for (;;) {
		// Into syscall
		if (wait_for_syscall(&status)) {
		exit_normaly:
			// Disable timer
			setitimer(ITIMER_REAL, &old_timer, &timer);
			gettimeofday(&tend, NULL); // Get finish time
			sigaction(SIGALRM, &sa_old, NULL);

			runtime = (tend.tv_sec - tbeg.tv_sec) * 1000000LL + tend.tv_usec -
				tbeg.tv_usec;

			return ExitStat(status, runtime/*opts->time_limit -
						timer.it_value.tv_sec * 1000000LL -
						timer.it_value.tv_usec*/);
		}
#ifdef __x86_64__
		int syscall = ptrace(PTRACE_PEEKUSER, cpid, sizeof(long)*ORIG_RAX);
#else
		int syscall = ptrace(PTRACE_PEEKUSER, cpid, sizeof(long)*ORIG_EAX);
#endif

		// If syscall is not allowed
		if (syscall == -1 || func(cpid, syscall, data) != 0) {

			// Disable timer
			setitimer(ITIMER_REAL, &old_timer, &timer);
			gettimeofday(&tend, NULL); // Get finish time
			sigaction(SIGALRM, &sa_old, NULL);

			// Kill process if still exist
			kill(cpid, SIGKILL);
			waitpid(cpid, &status, 0);

			runtime = (tend.tv_sec - tbeg.tv_sec) * 1000000LL + tend.tv_usec -
				tbeg.tv_usec;

			// If time limit exceeded
			// if (timer.it_value.tv_sec == 0 && timer.it_value.tv_usec == 0)
			if (runtime >= opts->time_limit)
				return ExitStat(status, runtime/*opts->time_limit -
						timer.it_value.tv_sec * 1000000LL -
						timer.it_value.tv_usec*/);

			return ExitStat(status, runtime/*opts->time_limit -
						timer.it_value.tv_sec * 1000000LL -
						timer.it_value.tv_usec*/,
					string("forbidden syscall: ")
					.append(toString((unsigned long long)syscall)));
		}

		// syscall returns
		if (wait_for_syscall(&status))
			goto exit_normaly;
	}
}

ExitStat thread_safe_run(const string& exec, vector<string> args,
		const struct options *opts, int (*func)(int, int, void*), void *data) {

	struct RuntimeInfo {
		int code;
		unsigned long long runtime;
		char message[1000];
	};

	SharedMemorySegment shm_sgmt(sizeof(RuntimeInfo));
	if (shm_sgmt.addr() == NULL)
		return ExitStat(-1, 0, string("Failed to create shared memory segment")
			+ strerror(errno));

	RuntimeInfo *rt_info = new (shm_sgmt.addr()) RuntimeInfo;

	pid_t child = fork();
	if (child == -1)
		return ExitStat(-1, 0, string("Failed to fork() - ") + strerror(errno));

	else if (child == 0) {
		ExitStat es = run(exec, args, opts, func, data);
		rt_info = (RuntimeInfo*)shmat(shm_sgmt.key(), NULL, 0);

		rt_info->code = es.code;
		rt_info->runtime = es.runtime;

		strncpy(rt_info->message, es.message.c_str(), sizeof(rt_info->message));
		rt_info->message[sizeof(rt_info->message) - 1] = '\0';

		_exit(0);
	}

	waitpid(child, NULL, 0);
	return ExitStat(rt_info->code, rt_info->runtime, rt_info->message);
}

} // namespace sandbox
