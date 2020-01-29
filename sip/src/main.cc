#include "commands.hh"
#include "sip_error.hh"
#include "sip_package.hh"

#include <atomic>
#include <cerrno>
#include <climits>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <simlib/concat_tostr.hh>
#include <simlib/debug.hh>
#include <simlib/directory.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/proc_stat_file_contents.hh>
#include <simlib/process.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/to_string.hh>
#include <simlib/working_directory.hh>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

/**
 * Parses options passed to Sip via arguments
 * @param argc like in main (will be modified to hold the number of non-option
 * parameters)
 * @param argv like in main (holds arguments)
 */
static void parseOptions(int& argc, char** argv) {
	STACK_UNWINDING_MARK;

	int new_argc = 1;

	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == '-') {
			if (0 == strcmp(argv[i], "-C") and
			    i + 1 < argc) { // Working directory
				if (chdir(argv[++i]) == -1) {
					eprintf("Error: chdir() - %s\n", strerror(errno));
					exit(1);
				}

			} else if (0 == strcmp(argv[i], "-h") or
			           0 == strcmp(argv[i], "--help")) { // Help
				commands::help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);

			} else if (0 == strcmp(argv[i], "-q") or
			           0 == strcmp(argv[i], "--quiet")) { // Quiet mode
				stdlog.open("/dev/null");

				// } else if (0 == strcmp(argv[i], "-v") or
				// 		0 == strcmp(argv[i], "--verbose")) { // Verbose mode
				// 	stdlog.use(stdout);
				// 	verbose = true;

			} else { // Unknown
				eprintf("Unknown option: '%s'\n", argv[i]);
			}

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
}

static void run_command(int argc, char** argv) {
	STACK_UNWINDING_MARK;

	ArgvParser args(argc, argv);
	StringView command = args.extract_next();

	// Commands
	if (command == "checker")
		return commands::checker(args);
	if (command == "clean")
		return commands::clean(args);
	if (command == "doc")
		return commands::doc(args);
	if (command == "gen")
		return commands::gen(args);
	if (command == "genin")
		return commands::genin(args);
	if (command == "genout")
		return commands::genout(args);
	if (command == "gentests")
		return commands::gen(args);
	if (command == "help")
		return commands::help(argv[0]);
	if (command == "init")
		return commands::init(args);
	if (command == "interactive")
		return commands::interactive(args);
	if (command == "label")
		return commands::label(args);
	if (command == "main-sol")
		return commands::main_sol(args);
	if (command == "mem")
		return commands::mem(args);
	if (command == "name")
		return commands::name(args);
	if (command == "prog")
		return commands::prog(args);
	if (command == "regen")
		return commands::regen(args);
	if (command == "regenin")
		return commands::regenin(args);
	if (command == "save")
		return commands::save(args);
	if (command == "statement")
		return commands::statement(args);
	if (command == "templ")
		return commands::template_command(args);
	if (command == "template")
		return commands::template_command(args);
	if (command == "test")
		return commands::test(args);
	if (command == "unset")
		return commands::unset(args);
	if (command == "zip")
		return commands::zip(args);

	throw SipError("unknown command: ", command);
}

int true_main(int argc, char** argv) {
	stdlog.use(stdout);
	stdlog.label(false);
	errlog.label(false);

	parseOptions(argc, argv);

	if (argc < 2) {
		commands::help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	try {
		run_command(argc, argv);
	} catch (const SipError& e) {
		errlog("\033[1;31mError\033[m: ", e.what());
		return 1;
	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	} catch (...) {
		ERRLOG_CATCH();
		return 1;
	}

	return 0;
}

void kill_every_child_process() {
	auto my_pid_str = to_string(getpid());
	for_each_dir_component("/proc/", [&](dirent* file) {
		auto pid_opt = str2num<pid_t>(file->d_name);
		if (not pid_opt or *pid_opt < 1)
			return; // Not a process
		pid_t pid = *pid_opt;

		auto proc_stat = ProcStatFileContents::get(pid);
		constexpr uint PPID_FID = 3;
		auto ppid_str = proc_stat.field(PPID_FID);
		if (ppid_str == my_pid_str) {
			stdlog("Killing child: ", pid);
			(void)kill(pid, SIGTERM);
		}
	});
}

void delete_zip_file_that_is_being_created() {
	auto zip_path_prefix = get_cwd();
	--zip_path_prefix.size; // Trim trailing '/'
	zip_path_prefix.append(".zip.");

	// Find and delete temporary zip file
	constexpr CStringView fd_dir = "/proc/self/fd/";
	for_each_dir_component(fd_dir, [&](dirent* file) {
		auto src = concat<64>(fd_dir, file->d_name);
		InplaceBuff<PATH_MAX + 1> dest; // +1 for trailing null byte
		auto rc =
		   readlink(src.to_cstr().data(), dest.data(), dest.max_static_size);
		if (rc == -1)
			return; // Ignore errors, as nothing reasonable can be done
		dest.size = rc;

		if (has_prefix(dest, zip_path_prefix))
			(void)unlink(dest.to_cstr());
	});
}

void cleanup_before_getting_killed() {
	try {
		kill_every_child_process();
	} catch (...) {
	}

	try {
		delete_zip_file_that_is_being_created();
	} catch (...) {
	}
}

namespace {
FileDescriptor signal_pipe_write_end;
} // namespace

static void signal_handler(int signum) {
	(void)write(signal_pipe_write_end, &signum, sizeof(int));
}

int main(int argc, char** argv) {
	// Prepare signal pipe
	FileDescriptor signal_pipe_read_end;
	{
		std::array<int, 2> pfd;
		if (pipe2(pfd.data(), O_CLOEXEC | O_NONBLOCK | O_DIRECT))
			THROW("pipe2()", errmsg());

		signal_pipe_read_end = pfd[0];
		signal_pipe_write_end = pfd[1];
	}

	// Signal control
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &signal_handler;
	// Intercept SIGINT and SIGTERM, to allow a dedicated thread to consume them
	if (sigaction(SIGINT, &sa, nullptr) or sigaction(SIGTERM, &sa, nullptr))
		THROW("sigaction()", errmsg());

	std::atomic_bool i_am_being_killed_by_signal = false;
	// Spawn signal-handling thread
	std::thread([&, signal_pipe_read_end = std::move(signal_pipe_read_end)] {
		int signum;
		// Wait for signal
		pollfd pfd {signal_pipe_read_end, POLLIN, 0};
		for (int rc;;) {
			rc = poll(&pfd, 1, -1);
			if (rc == 1)
				break; // Signal arrived

			assert(rc < 0);
			if (errno != EINTR)
				THROW("poll()");
		}

		if (pfd.revents & POLLHUP)
			return; // The main thread died

		i_am_being_killed_by_signal = true;
		int rc = read(signal_pipe_read_end, &signum, sizeof(int));
		assert(rc == sizeof(int));

		// print message: "Sip was just killed by signal %i - %s\n"
		{
			InplaceBuff<4096> buff("\nSip was just killed by signal ");
			buff.append(signum, " - ", sys_siglist[signum], '\n');
			write(STDERR_FILENO, buff.data(), buff.size);
			// Prevent other errors to show up in the console
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}

		try {
			cleanup_before_getting_killed();
		} catch (...) {
		}

		// Now kill the whole process group with the intercepted signal
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_DFL;
		// First unblock the blocked signal, so that it will kill the process
		(void)sigaction(signum, &sa, nullptr);
		(void)kill(0, signum);
	}).detach();

	int rc = true_main(argc, argv);
	if (not i_am_being_killed_by_signal)
		return rc;

	pause(); // Wait till the signal-handling thread kills the whole process
}
