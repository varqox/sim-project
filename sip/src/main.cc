#include "commands.h"
#include "sip_error.h"
#include "sip_package.h"

#include <signal.h>
#include <simlib/debug.h>
#include <simlib/filesystem.h>
#include <simlib/parsers.h>
#include <simlib/process.h>

/**
 * Parses options passed to Sip via arguments
 * @param argc like in main (will be modified to hold the number of non-option
 * parameters)
 * @param argv like in main (holds arguments)
 */
static void parseOptions(int &argc, char **argv) {
	STACK_UNWINDING_MARK;

	int new_argc = 1;

	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == '-') {
			// Working directory
			if (0 == strcmp(argv[i], "-C") and i + 1 < argc) {
				if (chdir(argv[++i]) == -1) {
					eprintf("Error: chdir() - %s\n", strerror(errno));
					exit(1);
				}

			// Help
			} else if (0 == strcmp(argv[i], "-h") or
					0 == strcmp(argv[i], "--help")) {
				commands::help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);

			// Quiet mode
			} else if (0 == strcmp(argv[i], "-q") or
					0 == strcmp(argv[i], "--quiet")) {
				stdlog.open("/dev/null");

			// // Verbose mode
			// } else if (0 == strcmp(argv[i], "-v") or
			// 		0 == strcmp(argv[i], "--verbose")) {
			// 	stdlog.use(stdout);
			// 	verbose = true;

			// Unknown
			} else {
				eprintf("Unknown option: '%s'\n", argv[i]);
			}

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
}

static void kill_signal_handler(int signum) {
	kill(0, signum); // Kill the whole process group

	// Some process might have changed their group, the above kill would leave them alive
	pid_t my_pid = getpid();
	// Kill all children processes
	forEachDirComponent("/proc/", [&](dirent* file) {
	#ifdef _DIRENT_HAVE_D_TYPE
		if (file->d_type == DT_DIR and isDigit(file->d_name)) {
	#else
		if (isDigit(file->d_name)) {
	#endif
			auto pid = strtoll(file->d_name);
			if (intentionalUnsafeStringView(toStr(my_pid)) == intentionalUnsafeStringView(getProcStat(pid, 3))) // 3 means PPID
				(void)kill(-pid, signum);
		}
	});

	dprintf(STDERR_FILENO, "Sip was just killed by signal %i - %s\n", signum, sys_siglist[signum]);
	_exit(1); // exit() is not safe to call from signal handler see man signal-safety
}

static void run_command(int argc, char **argv) {
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
	if (command == "statement")
		return commands::statement(args);
	if (command == "test")
		return commands::test(args);
	if (command == "zip")
		return commands::zip(args);

	throw SipError("unknown command: ", command);
}

int main(int argc, char **argv) {
	stdlog.use(stdout);
	stdlog.label(false);
	errlog.label(false);

	// Upon termination kill children processes
	{
		// Signal control
		struct sigaction sa;
		memset (&sa, 0, sizeof(sa));
		sa.sa_handler = &kill_signal_handler;

		(void)sigaction(SIGINT, &sa, nullptr);
		(void)sigaction(SIGTERM, &sa, nullptr);
		(void)sigaction(SIGQUIT, &sa, nullptr);
	}

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
