#include "commands.hh"
#include "sip_error.hh"
#include "sip_package.hh"

#include <signal.h>
#include <simlib/debug.hh>
#include <simlib/process.hh>

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

static void kill_signal_handler(int signum) {
	kill(0, signum); // Kill the whole process group

	// print message: "Sip was just killed by signal %i - %s\n"
	{
		InplaceBuff<4096> buff("Sip was just killed by signal ");
		buff.append(signum, " - ", sys_siglist[signum], '\n');
		write(STDERR_FILENO, buff.data(), buff.size);
	}

	auto zip_path_prefix = [] {
		InplaceBuff<PATH_MAX + 16> path;
		ssize_t rc = readlink("/proc/self/cwd", path.data(), PATH_MAX);
		if (rc == -1)
			return path; // Cannot read CWD

		path.size = rc;
		path.append(".zip.");
		return path;
	}();

	// Check for temporary zip file
	if (zip_path_prefix.size > 0) {
		// We don't touch stdin, stdout and stderr
		for (uint i = 3; i < 100; ++i) {
			InplaceBuff<64> src("/proc/self/fd/");
			src.append(i);

			constexpr size_t path_len =
			   PATH_MAX + 1; // This 1 is for trailing null
			InplaceBuff<path_len> path;
			ssize_t rc =
			   readlink(src.to_cstr().data(), path.data(), path_len - 1);
			if (rc == -1)
				continue; // Ignore errors

			path.size = rc;
			if (has_prefix(path, zip_path_prefix))
				(void)unlink(path.to_cstr().data());
		}
	}

	_exit(1); // exit() is not safe to call from signal handler see man
	          // signal-safety
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

int main(int argc, char** argv) {
	stdlog.use(stdout);
	stdlog.label(false);
	errlog.label(false);

	// Upon termination kill children processes
	{
		// Signal control
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
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
