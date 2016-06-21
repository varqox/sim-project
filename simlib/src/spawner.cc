#include "../include/spawner.h"

using std::array;
using std::string;

int Spawner::NormalTimer::cpid;
std::mutex Spawner::NormalTimer::lock;

string Spawner::receiveErrorMessage(int status, int fd) {
	string message;
	array<char, 4096> buff;
	// Read errors from fd
	ssize_t rc;
	while ((rc = read(fd, buff.data(), buff.size())) > 0)
		message.append(buff.data(), rc);

	if (message.size()) // Error in tracee
		THROW(message);

	if (WIFEXITED(status))
		message = concat("returned ", toString(WEXITSTATUS(status)));

	else if (WIFSIGNALED(status))
		message = concat("killed by signal ", toString(WTERMSIG(status)), " - ",
			strsignal(WTERMSIG(status)));

	return message;
};
