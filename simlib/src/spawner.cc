#include "../include/spawner.h"

#include <sys/syscall.h>

using std::array;
using std::string;
using std::vector;

string Spawner::receiveErrorMessage(const siginfo_t& si, int fd) {
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
		message = concat_tostr("returned ", si.si_status);
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

Spawner::ExitStat Spawner::run(CStringView exec, const vector<string>& args,
	const Spawner::Options& opts, CStringView working_dir)
{
	// Error stream from child (and wait_for_syscall()) via pipe
	int pfd[2];
	if (pipe2(pfd, O_CLOEXEC) == -1)
		THROW("pipe()", error(errno));

	int cpid = fork();
	if (cpid == -1)
		THROW("fork()", error(errno));

	else if (cpid == 0) {
		sclose(pfd[0]);
		runChild(exec, args, opts, working_dir, pfd[1], []{});
	}

	sclose(pfd[1]);
	Closer pipe0_closer(pfd[0]);

	// Wait for child to be ready
	siginfo_t si;
	rusage ru;
	syscall(SYS_waitid, P_PID, cpid, &si, WSTOPPED | WEXITED, &ru);
	// If something went wrong
	if (si.si_code != CLD_STOPPED)
		return ExitStat({0, 0}, si.si_code, si.si_status, ru, 0,
			receiveErrorMessage(si, pfd[0]));

	// Set up timer
	Timer timer(cpid, opts.real_time_limit);
	kill(cpid, SIGCONT);

	// Wait for death of the child
	syscall(SYS_waitid, P_PID, cpid, &si, WEXITED, &ru);

	timespec runtime = timer.stop_and_get_runtime();
	if (si.si_code != CLD_EXITED or si.si_status != 0)
		return ExitStat(runtime, si.si_code, si.si_status, ru, 0,
			receiveErrorMessage(si, pfd[0]));

	return ExitStat(runtime, si.si_code, si.si_status, ru, 0);
}
