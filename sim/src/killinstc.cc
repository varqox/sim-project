#include <chrono>
#include <cstring>
#include <simlib/debug.hh>
#include <simlib/logger.hh>
#include <simlib/process.hh>
#include <simlib/string_traits.hh>
#include <thread>
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
	stdlog("  --wait=t  Wait no longer than t seconds for a process to die "
	       "after sending signal (t may be a floating point decimal, t equal "
	       "to 0 means to wait indefinitely)");
	stdlog("  --kill-after=t");
	stdlog("            If a process does not die after t seconds, it will be "
	       "signaled with SIGKILL");
}

int main(int argc, char** argv) {
	stdlog.label(false);
	errlog.label(false);

	if (argc == 0) {
		help(nullptr);
		return 1;
	}

	bool parameters_error = false;
	std::optional<std::chrono::duration<double>> wait_timeout;
	bool kill_after_waiting = false;

	// Parse arguments
	for (int old_argc = argc, i = argc = 1; i < old_argc; ++i) {
		StringView arg(argv[i]);
		// Not an option
		if (!has_prefix(arg, "-")) {
			argv[argc++] = argv[i];
			continue;
		}

		// Options
		arg.remove_prefix(1); // remove '-'
		if (arg == "-wait") {
			// Wait
			wait_timeout = std::nullopt;
			kill_after_waiting = false;

		} else if (has_prefix(arg, "-wait=")) {
			// Wait no longer than
			wait_timeout = std::chrono::duration<double>(atof(arg.data() + 6));
			kill_after_waiting = false;
			if (*wait_timeout <= std::chrono::seconds(0)) {
				errlog("Too small value in option --wait=");
				parameters_error = true;
			}

		} else if (has_prefix(arg, "-kill-after")) {
			// Kill after
			wait_timeout = std::chrono::duration<double>(atof(arg.data() + 12));
			kill_after_waiting = true;
			if (*wait_timeout <= std::chrono::seconds(0)) {
				errlog("Too small value in option --kill-after=");
				parameters_error = true;
			}

		} else {
			// Unknown
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

	vector<std::string> exec_set;
	for (int i = 1; i < argc; ++i)
		exec_set.emplace_back(argv[i]);

	auto victims = find_processes_by_executable_path(exec_set);
	for (auto pid : victims)
		stdlog("Killing ", pid);

	kill_processes_by_exec(std::move(exec_set), wait_timeout,
	                       kill_after_waiting);
	return 0;
}
