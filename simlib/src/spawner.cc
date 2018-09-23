#include "../include/spawner.h"

#include <sys/syscall.h>

using std::array;
using std::string;
using std::vector;

string Spawner::receive_error_message(const siginfo_t& si, int fd) {
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
	default:
		THROW("Invalid siginfo_t.si_code: ", si.si_code);
	}

	return message;
};

Spawner::ExitStat Spawner::run(FilePath exec,
	const vector<string>& exec_args, const Spawner::Options& opts)
{
	// Error stream from child via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", errmsg());

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", errmsg());

	else if (cpid == 0) {
		sclose(pfd[0]);
		run_child(exec, exec_args, opts, pfd[1], []{});
	}

	sclose(pfd[1]);
	FileDescriptorCloser pipe0_closer(pfd[0]);

	// Wait for child to be ready
	siginfo_t si;
	rusage ru;
	if (syscall(SYS_waitid, P_PID, cpid, &si, WSTOPPED | WEXITED, &ru) == -1)
		THROW("waitid()", errmsg());

	// If something went wrong
	if (si.si_code != CLD_STOPPED)
		return ExitStat({0, 0}, {0, 0}, si.si_code, si.si_status, ru, 0,
			receive_error_message(si, pfd[0]));

	// Useful when exception is thrown
	auto kill_and_wait_child_guard = make_call_in_destructor([&] {
		kill(-cpid, SIGKILL);
		waitid(P_PID, cpid, &si, WEXITED);
	});

	// Set up timers
	Timer timer(cpid, opts.real_time_limit);
	CPUTimeMonitor cpu_timer(cpid, opts.cpu_time_limit);
	kill(cpid, SIGCONT); // There is only one process now, so '-' is not needed

	// Wait for death of the child
	timespec cpu_time {};
	waitid(P_PID, cpid, &si, WEXITED | WNOWAIT);

	// Get cpu_time
	cpu_timer.deactivate();
	cpu_time = cpu_timer.get_cpu_runtime();

	kill_and_wait_child_guard.cancel();
	syscall(SYS_waitid, P_PID, cpid, &si, WEXITED, &ru);
	timespec runtime = timer.stop_and_get_runtime(); // This have to be last
	                                                 // because it may throw

	if (si.si_code != CLD_EXITED or si.si_status != 0)
		return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru, 0,
			receive_error_message(si, pfd[0]));

	return ExitStat(runtime, cpu_time, si.si_code, si.si_status, ru, 0);
}
