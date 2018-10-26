#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/process.h>
#include <unistd.h>

using std::pair;
using std::string;
using std::vector;

void help(const char* program_name) {
	if (!program_name)
		program_name = "killinstc";

	stdlog("Usage: ", program_name, " [options] <file>");
	stdlog("Kill all process which are instances of <file>");
	stdlog("");
	stdlog("Options:");
	stdlog("  --wait    Wait for a process to die after sending the signal");
	stdlog("  --wait=t  Wait no longer than t seconds for a process to die after"
				" sending signal (t may be a floating point decimal, t equal"
				" to 0 means to wait indefinitely)");
	stdlog("  --kill-after=t");
	stdlog("            If a process does not die after t seconds, it will be"
				" signaled with SIGKILL");

}

int main(int argc, char **argv) {
	stdlog.label(false);
	errlog.label(false);

	if (argc == 0) {
		help(nullptr);
		return 1;
	}

	bool parameters_error = false;
	uint64_t kill_after = 0; // in usec
	int64_t wait_interval = 0; // in usec
	bool wait = false;

	// Parse arguments
	for (int old_argc = argc, i = argc = 1; i < old_argc; ++i) {
		StringView arg(argv[i]);
		// Not an option
		if (!hasPrefix(arg, "-")) {
			argv[argc++] = argv[i];
			continue;
		}

		// Options
		arg.removePrefix(1); // remove '-'
		// Wait
		if (arg == "-wait")
			wait = true;

		// Wait no longer than
		else if (hasPrefix(arg, "-wait=")) {
			wait = true;
			wait_interval = atof(arg.data() + 6) * 1e6;
			if (wait_interval <= 0) {
				errlog("Too small value in option --wait=");
				parameters_error = true;
			}

		// Kill after
		} else if (hasPrefix(arg, "-kill-after=")) {
			kill_after = atof(arg.data() + 12) * 1e6;
			if (kill_after == 0) {
				errlog("Too small value in option --kill-after=");
				parameters_error = true;
			}


		// Unknown
		} else {
			errlog("Unrecognized option: `%s`\n", argv[i]);
			parameters_error = true;
		}
	}

	// Errors
	if (parameters_error)
		return 1;

	// Only options, nothing to do
	 if (argc == 1) {
		help(argv[0]);
		return 1;
	}

	constexpr uint START_TIME_FID = 21; // Number of the field in
	                                    // /proc/[pid]/stat that contains
	                                    // process start time

	try {
		// Collect pids
		using Victim = pair<pid_t, string>;
		vector<Victim> victims;
		{
			vector<std::string> exec_set;
			for (int i = 1; i < argc; ++i)
				exec_set.emplace_back(argv[i]);

			auto x = findProcessesByExec(std::move(exec_set), false);
			for (pid_t pid : x)
				try {
					victims.emplace_back(pid, getProcStat(pid, START_TIME_FID));
				} catch (const std::runtime_error& e) {
					errlog(e.what());
				}
		}

		if (victims.empty())
			return 0;

		if (wait || kill_after) {
			// If one of the processes has just appeared, wait for a clock tick
			// to distinguish it from a process that may appear (just after
			// killing) with the same pid
			Victim& oldest = *std::max_element(victims.begin(), victims.end(),
				[](const Victim& a, const Victim& b) {
					return StrNumCompare()(a.second, b.second);
				});
			if (oldest.second == getProcStat(getpid(), START_TIME_FID))
				usleep(ceil(1.0e6 / sysconf(_SC_CLK_TCK))); // Wait for a tick
		}

		// Send signals
		for (int i = 0; i < (int)victims.size(); ++i) {
			auto&& vic = victims[i];
			stdlog(vic.first, " <- SIGTERM");

			if (kill(vic.first, SIGTERM) == -1) {
				// Unsuccessful kill
				if (errno != ESRCH)
					errlog("kill(", vic.first, ")", errmsg());

				swap(victims[i--], victims.back());
				victims.pop_back();
			}
		}

		if (kill_after)
			wait_interval = kill_after;

		wait = (wait && wait_interval == 0); // Wait now tells whether to wait
		                                     // indefinitely
		uint sleep_interval = 0.25e6; // 0.25 s
		while (victims.size() && (wait || wait_interval > 0)) {
			if (!wait) {
				sleep_interval = meta::min(wait_interval, 0.1e6); // 0.1 s
				wait_interval -= sleep_interval;
			}

			usleep(sleep_interval);

			// Remove dead processes (victims)
			for (int i = 0; i < (int)victims.size(); ++i) {
				auto&& vic = victims[i];

				try {
					if (vic.second == getProcStat(vic.first, START_TIME_FID))
						continue; // Process is still not dead

				} catch (std::runtime_error const&) {}

				// Process has died
				swap(victims[i--], victims.back());
				victims.pop_back();
			}
		}

		// Kill remaining processes (victims)
		if (kill_after)
			for (auto vic : victims) {
				stdlog(vic.first, " <- SIGKILL");
				(void)kill(vic.first, SIGKILL);
			}

	} catch(const std::exception& e) {
		errlog(e.what());
		return 1;
	}

	return 0;
}
