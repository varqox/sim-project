#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/process.h>

using std::vector;

int main(int argc, char **argv) {
	// TODO: add quiet and verbose mode and signal choosing
	if (argc != 2) {
		eprintf("Usage: %s <file>\nKill all process which are instances of "
			"<file>\n", argv[0]);
		return 1;
	}

	try {
		vector<pid_t> vec = findProcessesByExec(argv[1]);
		for (auto& pid : vec) {
			printf("%i <- SIGTERM\n", pid);
			if (kill(pid, SIGTERM) == -1)
				eprintf("kill(%i)%s\n", pid, error(errno).c_str());
		}

	} catch(const std::exception& e) {
		eprintf("%s\n", e.what());
		return 1;
	}

	return 0;
}
