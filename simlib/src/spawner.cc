#include "../include/spawner.h"
#include "../include/time.h"

#include <sys/syscall.h>

using std::array;
using std::string;
using std::vector;

string Spawner::receive_error_message(const siginfo_t& si, int fd) {
	STACK_UNWINDING_MARK;

	string message;
	array<char, 4096> buff;
	// Read errors from fd
	ssize_t rc;
	while ((rc = read(fd, buff.data(), buff.size())) > 0)
		message.append(buff.data(), rc);

	if (message.size()) // Error in tracee
		THROW(message);

	switch (si.si_code) {
	case CLD_EXITED:
		message = concat_tostr("exited with ", si.si_status);
		break;
	case CLD_KILLED:
		message = concat_tostr("killed by signal ", si.si_status, " - ",
		                       strsignal(si.si_status));
		break;
	case CLD_DUMPED:
		message = concat_tostr("killed and dumped by signal ", si.si_status,
		                       " - ", strsignal(si.si_status));
		break;
	default: THROW("Invalid siginfo_t.si_code: ", si.si_code);
	}

	return message;
};

void Spawner::Timer::handle_timeout(int, siginfo_t* si, void*) noexcept {
	int errnum = errno;
	Data& data = *(Data*)si->si_value.sival_ptr;
	try {
		data.timeouter(data.pid);
	} catch (...) {
	}
	errno = errnum;
}

void Spawner::Timer::delete_timer() noexcept {
	if (timer_is_active) {
		timer_is_active = false;
		(void)timer_delete(timerid);
	}
}

Spawner::Timer::Timer(pid_t pid, std::chrono::nanoseconds time_limit,
                      TimeoutHandler timeouter)
   : data {pid, std::move(timeouter)}, tlimit(to_timespec(time_limit)) {
	STACK_UNWINDING_MARK;

	throw_assert(time_limit >= decltype(time_limit)::zero());

	if (time_limit == std::chrono::nanoseconds::zero()) {
		if (clock_gettime(CLOCK_MONOTONIC, &begin_point))
			THROW("clock_gettime()", errmsg());

	} else {
		const int USED_SIGNAL = SIGRTMIN;

		// Install (timeout) signal handler
		struct sigaction sa;
		sa.sa_flags = SA_SIGINFO | SA_RESTART;
		sa.sa_sigaction = handle_timeout;
		if (sigaction(USED_SIGNAL, &sa, nullptr))
			THROW("signaction()", errmsg());

		// Prepare timer
		sigevent sev;
		memset(&sev, 0, sizeof(sev));
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = USED_SIGNAL;
		sev.sigev_value.sival_ptr = &data;
		if (timer_create(CLOCK_MONOTONIC, &sev, &timerid))
			THROW("timer_create()", errmsg());

		timer_is_active = true;

		// Arm timer
		itimerspec its {{0, 0}, tlimit};
		if (timer_settime(timerid, 0, &its, nullptr)) {
			int errnum = errno;
			delete_timer();
			THROW("timer_settime()", errmsg(errnum));
		}
	}
}

std::chrono::nanoseconds Spawner::Timer::stop_and_get_runtime() {
	STACK_UNWINDING_MARK;

	if (tlimit.tv_sec == 0 and tlimit.tv_nsec == 0) {
		timespec end_point;
		if (clock_gettime(CLOCK_MONOTONIC, &end_point))
			THROW("clock_gettime()", errmsg());
		return to_nanoseconds(end_point - begin_point);
	}

	itimerspec its {{0, 0}, {0, 0}}, old;
	if (timer_settime(timerid, 0, &its, &old))
		THROW("timer_settime()", errmsg());

	delete_timer();
	return to_nanoseconds(tlimit - old.it_value);
}

void Spawner::CPUTimeMonitor::handler(int, siginfo_t* si, void*) noexcept {
	int errnum = errno;

	Data& data = *(Data*)si->si_value.sival_ptr;
	timespec ts;
	if (clock_gettime(data.cid, &ts) or ts >= data.cpu_abs_time_limit) {
		// Failed to get time or cpu_time_limit expired
		try {
			data.timeouter(data.pid);
		} catch (...) {
		} // TODO: distinguish error and timeout e.g. saving error code in data
		  // and throwing from get_cpu_runtime()

	} else {
		// There is still time left
		itimerspec its {
		   {0, 0},
		   meta::max(data.cpu_abs_time_limit - ts,
		             timespec {0, (int)0.01e9}) // Min. wait duration: 0.01 s
		};
		timer_settime(data.timerid, 0, &its, nullptr);
	}

	errno = errnum;
}

Spawner::CPUTimeMonitor::CPUTimeMonitor(pid_t pid,
                                        std::chrono::nanoseconds cpu_time_limit,
                                        TimeoutHandler timeouter)
   : data {pid, {}, to_timespec(cpu_time_limit), {}, {}, std::move(timeouter)} {
	STACK_UNWINDING_MARK;

	throw_assert(cpu_time_limit >= decltype(cpu_time_limit)::zero());

	if (clock_getcpuclockid(pid, &data.cid))
		THROW("clock_getcpuclockid()", errmsg());

	if (clock_gettime(data.cid, &data.cpu_time_at_start))
		THROW("clock_gettime()", errmsg());

	if (cpu_time_limit == std::chrono::nanoseconds::zero())
		return; // No limit is set - nothing to do

	// Update the cpu_abs_time_limit to reflect cpu_time_at_start
	data.cpu_abs_time_limit += data.cpu_time_at_start;

	const int USED_SIGNAL = SIGRTMIN + 1;

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = handler;
	sigfillset(&sa.sa_mask); // Prevent interrupting
	if (sigaction(USED_SIGNAL, &sa, nullptr))
		THROW("sigaction()", errmsg());

	// Prepare timer
	sigevent sev;
	memset(&sev, 0, sizeof(sev));
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = USED_SIGNAL;
	sev.sigev_value.sival_ptr = &data;
	if (timer_create(CLOCK_MONOTONIC, &sev, &data.timerid))
		THROW("timer_create()", errmsg());

	timer_is_active = true;

	itimerspec its {{0, 0}, to_timespec(cpu_time_limit)};
	if (timer_settime(data.timerid, 0, &its, nullptr)) {
		int errnum = errno;
		deactivate();
		THROW("timer_settime()", errmsg(errnum));
	}
}

void Spawner::CPUTimeMonitor::deactivate() noexcept {
	if (timer_is_active) {
		timer_is_active = false;
		timer_delete(data.timerid);
	}
}

std::chrono::nanoseconds Spawner::CPUTimeMonitor::get_cpu_runtime() {
	STACK_UNWINDING_MARK;

	timespec ts;
	if (clock_gettime(data.cid, &ts))
		THROW("clock_gettime()", errmsg());

	return to_nanoseconds(ts - data.cpu_time_at_start);
}

Spawner::ExitStat Spawner::run(FilePath exec, const vector<string>& exec_args,
                               const Spawner::Options& opts,
                               const std::function<void()>& do_after_fork) {
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (opts.real_time_limit.has_value() and
	    opts.real_time_limit.value() <= 0ns)
		THROW("If set, real_time_limit has to be greater than 0");

	if (opts.cpu_time_limit.has_value() and opts.cpu_time_limit.value() <= 0ns)
		THROW("If set, cpu_time_limit has to be greater than 0");

	if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0)
		THROW("If set, memory_limit has to be greater than 0");

	// Error stream from child via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", errmsg());

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", errmsg());

	else if (cpid == 0) {
		close(pfd[0]);
		run_child(exec, exec_args, opts, pfd[1], [] {});
	}

	close(pfd[1]);
	FileDescriptorCloser pipe0_closer(pfd[0]);

	// Wait for child to be ready
	siginfo_t si;
	rusage ru;
	if (syscall(SYS_waitid, P_PID, cpid, &si, WSTOPPED | WEXITED, &ru) == -1)
		THROW("waitid()", errmsg());

	// If something went wrong
	if (si.si_code != CLD_STOPPED)
		return ExitStat(0ns, 0ns, si.si_code, si.si_status, ru, 0,
		                receive_error_message(si, pfd[0]));

	// Useful when exception is thrown
	auto kill_and_wait_child_guard = make_call_in_destructor([&] {
		kill(-cpid, SIGKILL);
		waitid(P_PID, cpid, &si, WEXITED);
	});

	do_after_fork();

	// Set up timers
	Timer timer(cpid, opts.real_time_limit.value_or(0ns));
	CPUTimeMonitor cpu_timer(cpid, opts.cpu_time_limit.value_or(0ns));
	kill(cpid, SIGCONT); // There is only one process now, so '-' is not needed

	// Wait for death of the child
	waitid(P_PID, cpid, &si, WEXITED | WNOWAIT);

	// Get cpu_time
	cpu_timer.deactivate();
	auto cpu_time = cpu_timer.get_cpu_runtime();

	kill_and_wait_child_guard.cancel();
	syscall(SYS_waitid, P_PID, cpid, &si, WEXITED, &ru);
	auto runtime = timer.stop_and_get_runtime(); // This have to be last
	                                             // because it may throw

	if (si.si_code != CLD_EXITED or si.si_status != 0)
		return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru, 0,
		                receive_error_message(si, pfd[0]));

	return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru, 0);
}

void Spawner::run_child(FilePath exec,
                        const std::vector<std::string>& exec_args,
                        const Options& opts, int fd,
                        const std::function<void()>& do_before_exec) noexcept {
	STACK_UNWINDING_MARK;
	// Sends error to parent
	auto send_error_and_exit = [fd](int errnum, CStringView str) {
		send_error_message_and_exit(fd, errnum, str);
	};

	// Create new process group (useful for killing the whole process group)
	if (setpgid(0, 0))
		send_error_and_exit(errno, "setpgid()");

	using std::chrono_literals::operator""ns;

	if (opts.real_time_limit.has_value() and
	    opts.real_time_limit.value() <= 0ns) {
		send_error_message_and_exit(
		   fd, "If set, real_time_limit has to be greater than 0");
	}

	if (opts.cpu_time_limit.has_value() and
	    opts.cpu_time_limit.value() <= 0ns) {
		send_error_message_and_exit(
		   fd, "If set, cpu_time_limit has to be greater than 0");
	}

	if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0) {
		send_error_message_and_exit(
		   fd, "If set, memory_limit has to be greater than 0");
	}

	// Convert exec_args
	const size_t len = exec_args.size();
	const char* args[len + 1];
	args[len] = nullptr;

	for (size_t i = 0; i < len; ++i)
		args[i] = exec_args[i].c_str();

	// Change working directory
	if (not isOneOf(opts.working_dir, "", ".", "./")) {
		if (chdir(opts.working_dir.c_str()) == -1)
			send_error_and_exit(errno, "chdir()");
	}

	// Set virtual memory and stack size limit (to the same value)
	if (opts.memory_limit.has_value()) {
		struct rlimit limit;
		limit.rlim_max = limit.rlim_cur = opts.memory_limit.value();
		if (setrlimit(RLIMIT_AS, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_AS)");
		if (setrlimit(RLIMIT_STACK, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_STACK)");
	}

	using std::chrono_literals::operator""ns;
	using std::chrono_literals::operator""s;
	using std::chrono::duration_cast;
	using std::chrono::nanoseconds;
	using std::chrono::seconds;

	auto set_cpu_rlimit = [&](nanoseconds cpu_tl) {
		// Limit below is useful when spawned process becomes orphaned
		rlimit limit;
		limit.rlim_cur = limit.rlim_max =
		   duration_cast<seconds>(cpu_tl + 1.5s)
		      .count(); // + 1.5 to avoid premature death

		if (setrlimit(RLIMIT_CPU, &limit))
			send_error_and_exit(errno, "setrlimit(RLIMIT_CPU)");
	};

	// Set CPU time limit [s]
	if (opts.cpu_time_limit.has_value())
		set_cpu_rlimit(opts.cpu_time_limit.value());
	else if (opts.real_time_limit.has_value())
		set_cpu_rlimit(opts.real_time_limit.value());

	// Change stdin
	if (opts.new_stdin_fd < 0) {
		close(STDIN_FILENO);
	} else if (opts.new_stdin_fd != STDIN_FILENO) {
		while (dup2(opts.new_stdin_fd, STDIN_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");
	}

	// Change stdout
	if (opts.new_stdout_fd < 0) {
		close(STDOUT_FILENO);
	} else if (opts.new_stdout_fd != STDOUT_FILENO) {
		while (dup2(opts.new_stdout_fd, STDOUT_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");
	}

	// Change stderr
	if (opts.new_stderr_fd < 0) {
		close(STDERR_FILENO);
	} else if (opts.new_stderr_fd != STDERR_FILENO) {
		while (dup2(opts.new_stderr_fd, STDERR_FILENO) == -1)
			if (errno != EINTR)
				send_error_and_exit(errno, "dup2()");
	}

	// Close file descriptors that are not needed to be open (for security
	// reasons)
	{
		Directory dir("/proc/self/fd");
		if (dir == nullptr)
			send_error_and_exit(errno, "opendir()");

		auto fd_str = toStr(fd);
		auto fd_cstr = fd_str.to_cstr().c_str();

		auto dir_fd = dirfd(dir);
		auto dir_fd_str = (dir_fd < 0 ? fd_str : toStr(dir_fd));
		auto dir_fd_cstr = dir_fd_str.to_cstr().c_str();

#if __cplusplus > 201402L
#warning "Use below meta::toStr<> instead of AS_STR()"
#endif

#define AS_STR(x) AS_STR_IMPL(x)
#define AS_STR_IMPL(x) #x
		std::array<const char*, 5> permitted_fds {{
		   dir_fd_cstr,
		   fd_cstr, // Needed in case of errors (it has FD_CLOEXEC flag set)
		   (opts.new_stdin_fd < 0 ? fd_cstr : AS_STR(STDIN_FILENO)),
		   (opts.new_stdout_fd < 0 ? fd_cstr : AS_STR(STDOUT_FILENO)),
		   (opts.new_stderr_fd < 0 ? fd_cstr : AS_STR(STDERR_FILENO)),
		}};
#undef AS_STR_IMPL
#undef AS_STR

		forEachDirComponent(
		   dir,
		   [&](dirent* file) {
			   for (auto&& str : permitted_fds)
				   if (strcmp(str, file->d_name) == 0)
					   return;

			   close(atoi(file->d_name));
		   },
		   [&] { send_error_and_exit(errno, "readdir()"); });
	}

	try {
		do_before_exec();
	} catch (std::exception& e) {
		send_error_message_and_exit(fd, CStringView(e.what()));
	}

	// Signal parent process that child is ready to execute @p exec
	kill(getpid(), SIGSTOP);

	execvp(exec, const_cast<char* const*>(args));
	int errnum = errno;

	// execvp() failed
	if (exec.size() <= PATH_MAX) {
		send_error_and_exit(errnum,
		                    intentionalUnsafeCStringView(
		                       concat<PATH_MAX + 20>("execvp('", exec, "')")));
	} else {
		send_error_and_exit(
		   errnum,
		   intentionalUnsafeCStringView(concat<PATH_MAX + 20>(
		      "execvp('", exec.to_cstr().substring(0, PATH_MAX), "...')")));
	}
}
