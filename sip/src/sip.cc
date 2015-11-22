#include "commands.h"
#include "sip.h"

#include "simlib/include/debug.h"

#include <unistd.h>

using std::string;
using std::vector;

unsigned VERBOSITY = 1; // 0 - quiet, 1 - normal, 2 or more - verbose
ProblemConfig pconf;

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
	puts("  verify [sol...]       Run inver and solutions [sol...] on tests (all solutions by default) (compiles solutions if necessary)");
	puts("  prog [sol...]         Compile solutions [sol...] (all solutions by default)");
	puts("  prepare               Prepare package to contest: gentests, genout");
	puts("  package               Prepare package to archiving: remove unnecessary files (compiled programs, generated tests, etc.)");
	puts("  init [directory] [name]");
	puts("                        Initialise Sip package in [directory] (by default current working directory) if [name] is specified, set problem name to it");
	puts("  gentests              Generate tests inputs: compile generators, generate tests");
	puts("  genout [sol]          Generate tests outputs using solution [sol] (main solution by default)");
	puts("  add-sol [sol...]      Add solutions [sol...] to package");
	puts("  rm-sol [sol...]       Remove solutions [sol...] from package");
	puts("  main-sol [sol]        If [sol] is specified set main solution to [sol], otherwise print main solution");
	puts("  set <name> <value>    Set <name> to <value> (e.g. 'sip set tag sum' sets problem tag to sum)");
	puts("");
	puts("Options:");
	puts("  -C <directory>         Change working directory to <directory> before doing anything");
	puts("  -h, --help             Display this information");
	// puts("  -m MEM_LIMIT, --memory-limit=MEM_LIMIT");
	// puts("                         Set problem memory limit to MEM_LIMIT in kB");
	// puts("  -n NAME, --name=NAME   Set problem name to NAME (cannot be empty)");
	// puts("  -q, --quiet            Quiet mode");
	// puts("  -t TAG, --tag=TAG      Set problem tag to TAG (cannot be empty)");
	// puts("  -tl VAL, --time-limit=VAL");
	// puts("                         Set time limit VAL in usec on every test, disables automatic time limit setting (only if VAL > 0)");
	puts("  -v, --verbose          Verbose mode");
	puts("");
	puts("Sip package tree:");
	puts("   main/                 Root package folder");
	puts("   |-- check/            Checker folder - holds checker");
	puts("   |-- doc/              Documents folder - holds problem statement, elaboration, ...");
	puts("   |-- prog/             Solutions folder - holds solutions");
	puts("   |-- tests/            Tests folder - holds tests");
	puts("   |-- utils/            Utilities folder - holds generators, inver and other stuff used by Sip");
	puts("   |-- config.conf       package config file - holds package config");
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
			if (0 == strcmp(argv[i], "-C") && i + 1 < argc &&
					chdir(argv[++i]) == -1) {
				eprintf("Error: chdir() - %s\n", strerror(errno));
				exit(1);
			}

			// Help
			else if (0 == strcmp(argv[i], "-h") ||
					0 == strcmp(argv[i], "--help")) {
				help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);
			}

			// Memory limit
			/*else if ((0 == strcmp(argv[i], "-m") ||
					0 == strcmp(argv[i], "--memory-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp))
					eprintf("Wrong memory limit\n");
				else {
					MEMORY_LIMIT = tmp;
					SET_MEMORY_LIMIT = true;
				}

			} else if (isPrefix(argv[i], "--memory-limit=")) {
				if (1 > sscanf(argv[i] + 15, "%llu", &tmp))
					eprintf("Wrong memory limit\n");
				else {
					MEMORY_LIMIT = tmp;
					SET_MEMORY_LIMIT = true;
				}
			}*/

			// Problem name
			/*else if ((0 == strcmp(argv[i], "-n") ||
					0 == strcmp(argv[i], "--name")) && i + 1 < argc)
				PROBLEM_NAME = argv[++i];

			else if (isPrefix(argv[i], "--name="))
				PROBLEM_NAME = string(argv[i]).substr(7);*/

			// Quiet mode
			else if (0 == strcmp(argv[i], "-q") ||
					0 == strcmp(argv[i], "--quiet"))
				VERBOSITY = 0;

			// Tag
			/*else if ((0 == strcmp(argv[i], "-t") ||
					0 == strcmp(argv[i], "--tag")) && i + 1 < argc)
				PROBLEM_TAG = argv[++i];

			else if (isPrefix(argv[i], "--tag="))
				PROBLEM_TAG = string(argv[i]).substr(6);*/

			// Time limit
			/*else if ((0 == strcmp(argv[i], "-tl") ||
					0 == strcmp(argv[i], "--time-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp))
					eprintf("Wrong time limit\n");
				else
					TIME_LIMIT = tmp;

			} else if (isPrefix(argv[i], "--time-limit=")) {
				if (1 > sscanf(argv[i] + 17, "%llu", &tmp))
					eprintf("Wrong time limit\n");
				else
					TIME_LIMIT = tmp;
			}*/

			// Verbose mode
			else if (0 == strcmp(argv[i], "-v") ||
					0 == strcmp(argv[i], "--verbose"))
				VERBOSITY = 2;

			// Unknown
			else
				eprintf("Unknown option: '%s'\n", argv[i]);

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
}

int main(int argc, char **argv) {
	stdlog.use(stdout);
	stdlog.label(false);
	error_log.label(false);

	parseOptions(argc, argv);

	if(argc < 2) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	try {
		int rc;
		// Commands
		if (strcmp(argv[1], "init") == 0)
			rc = command::init(argc - 1, argv + 1);

		else if (strcmp(argv[1], "set") == 0)
			rc = command::set(argc - 1, argv + 1);

		// Unknown command
		else {
			error_log("Unknown command: ", argv[1]);
			return 2;
		}

		return rc;

	} catch (const std::exception& e) {
		error_log("\e[1;31mError:\e[m ", e.what());
		return 1;
	}

	return 0;
}
