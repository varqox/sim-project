#include "commands.h"

#include <signal.h>
#include <simlib/debug.h>
#include <simlib/filesystem.h>
#include <simlib/parsers.h>
#include <simlib/process.h>

bool verbose = false;

/**
 * @brief Displays help
 */
static void help(const char* program_name) {
	if (program_name == NULL)
		program_name = "sip";

	printf("Usage: %s [options] <command> [<args>]\n", program_name);
	puts("Sip is a tool for preparing and managing Sim problem packages");
	puts("");
	puts("Commands:");
	// puts("  verify [sol...]       Run inver and solutions [sol...] on tests (all solutions by default) (compiles solutions if necessary)");
	puts("  checker [value]       If [value] is specified set checker to [value], otherwise print its current value");
	puts("  clean [arg...]        Prepare package to archiving: remove unnecessary files (compiled programs, latex logs, etc.)");
	puts("                        Allowed args:");
	puts("                          tests - removes generated tests");
	puts("  doc [watch]           Compiles latex statements (if there is any). If watch is specified as an argument then all statement files will be watched and recompiled on any change");
	puts("  gen [force]           Alias to gentests");
	puts("  genout [sol]          Generate tests outputs using solution [sol] (main solution by default)");
	puts("  gentests [force]      Generate tests inputs: compile generators, generate tests. If force is specified then tests that are not specified in Sipfile are removed");
	puts("  init [directory] [name]");
	puts("                        Initialize Sip package in [directory] (by default current working directory) if [name] is specified, set problem name to it");
	puts("  label [value]         If [value] is specified set label to [value], otherwise print its current value");
	puts("  main-sol [sol]        If [sol] is specified set main solution to [sol], otherwise print main solution");
	puts("  mem [value]           If [value] is specified set memory limit to [value] MB, otherwise print its current value");
	puts("  name [value]          If [value] is specified set name to [value], otherwise print its current value");
	// puts("  package               The same as clean");
	// puts("  prepare               Prepare package to contest: gentests, genout");
	puts("  prog [sol...]         Compile solutions [sol...] (all solutions by default). [sol] has the same meaning as in command 'test'");
	puts("  statement [value]     If [value] is specified set statement to [value], otherwise print its current value");
	puts("  test [sol...]         Run solutions [sol...] on tests (only main solution by default) (compiles solutions if necessary). If [sol] is a path to a solution then it is used, otherwise all solutions that have [sol] as a subsequence are used.");
	puts("  zip [clean args...]   Run clean command and compress the package into zip (named after the current directory) within upper directory.");
	puts("");
	puts("Options:");
	puts("  -C <directory>         Change working directory to <directory> before doing anything");
	puts("  -h, --help             Display this information");
	puts("  -q, --quiet            Quiet mode");
	puts("  -v, --verbose          Verbose mode (the default option)");
	puts("");
	puts("Sip package tree:");
	puts("   main/                 Main package folder");
	puts("   |-- check/            Checker folder - holds checker");
	puts("   |-- doc/              Documents folder - holds problem statement, elaboration, ...");
	puts("   |-- prog/             Solutions folder - holds solutions");
	puts("   |-- tests/            Tests folder - holds tests");
	puts("   |-- utils/            Utilities folder - holds generators, inver and other stuff used by Sip");
	puts("   |-- Simfile           Simfile - holds package config");
	puts("   `-- Sipfile           Sip file - holds Sip configuration and rules");
}

/**
 * Parses options passed to Sip via arguments
 * @param argc like in main (will be modified to hold the number of non-option
 * parameters)
 * @param argv like in main (holds arguments)
 */
static void parseOptions(int &argc, char **argv) {
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
				help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);

			// Quiet mode
			} else if (0 == strcmp(argv[i], "-q") or
					0 == strcmp(argv[i], "--quiet")) {
				stdlog.open("/dev/null");

			// Verbose mode
			} else if (0 == strcmp(argv[i], "-v") or
					0 == strcmp(argv[i], "--verbose")) {
				stdlog.use(stdout);
				verbose = true;

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

	exit(1);
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

	if(argc < 2) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	try {
		ArgvParser args(argc, argv);
		StringView command = args.extract_next();
		// Commands
		if (command == "checker")
			return command::checker(args);
		if (command == "clean")
			return command::clean(args);
		if (command == "doc")
			return command::doc(args);
		if (command == "gen")
			return command::gentests(args);
		if (command == "genout")
			return command::genout(args);
		if (command == "gentests")
			return command::gentests(args);
		if (command == "init")
			return command::init(args);
		if (command == "label")
			return command::label(args);
		if (command == "main-sol")
			return command::main_sol(args);
		if (command == "mem")
			return command::mem(args);
		if (command == "name")
			return command::name(args);
		// if (command == "package")
			// return command::package(args);
		// if (command == "prepare")
			// return command::prepare(args);
		if (command == "prog")
			return command::prog(args);
		if (command == "statement")
			return command::statement(args);
		if (command == "test")
			return command::test(args);
		if (command == "zip")
			return command::zip(args);

		// Unknown command
		errlog("Unknown command: ", command);
		return 2;

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	} catch (...) {
		ERRLOG_CATCH();
	}

	return 1;
}
