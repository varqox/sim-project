#include "conver.h"
#include "convert_package.h"
#include "default_checker_dump.h"
#include "set_limits.h"
#include "validate_package.h"

#include "../include/debug.h"
#include "../include/process.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using std::string;
using std::vector;

// Global variables
UniquePtr<TemporaryDirectory> tmp_dir;
bool VERBOSE = false, QUIET = false, GEN_OUT = false, VALIDATE_OUT = false;
bool USE_CONF = false, FORCE_AUTO_LIMIT = false;
unsigned long long MEMORY_LIMIT = 64 << 10; // 64 MB (in kB)
unsigned long long HARD_TIME_LIMIT = 10 * 1000000; // 10s
unsigned long long TIME_LIMIT = 0; // Not set
UniquePtr<directory_tree::node> package_tree_root;
ProblemConfig conf_cfg;

static bool SET_MEMORY_LIMIT = false; // true if is set in options
static string PROBLEM_NAME, DEST_NAME, PROBLEM_TAG;

string ProblemConfig::dump() {
	string res;
	Appender<string> app(res);
	app(name)('\n')(tag)('\n')(statement)('\n')(checker)('\n')(solution)('\n')
		(toString(memory_limit))('\n');

	// Tests
	for (vector<Group>::iterator i = test_groups.begin(),
			end = test_groups.end(); i != end; ++i) {
		if (i->tests.empty())
			continue;

		sort(i->tests.begin(), i->tests.end(), ProblemConfig::TestCmp());
		app('\n')(i->tests[0].name)(' ')
			(usecToSecStr(i->tests[0].time_limit, 2))(' ')(toString(i->points))
			('\n');

		for (vector<Test>::iterator j = ++i->tests.begin(); j != i->tests.end();
				++j)
			app(j->name)(' ')(usecToSecStr(j->time_limit, 2))('\n');
	}
	return res;
}

/**
 * @brief Displays help
 */
static void help() {
	eprintf("\
Usage: conver [options] problem_package\n\
Convert problem_package to sim_problem_package\n\
  problem_package have to be archive (.zip, .tar.gz, .tgz, .7z) or directory\n\
\n\
Options:\n\
  -fal, --force-auto-limit\n\
                         Force automatic time limit setting\n\
  -g, --gen-out, --generate-out\n\
                         Generate .out files (tests), if exist override, disables -vo\n\
  -h, --help             Display this information\n\
  -m MEM_LIMIT, --memory-limit=MEM_LIMIT\n\
                         Set problem memory limit MEM_LIMIT in kB\n\
  -mt <VAL>, --max-time-limit=<VAL>\n\
                         Set hard max time limit VAL in usec\n\
  -n NAME, --name=NAME   Set problem name to NAME (cannot be empty)\n\
  -o DESTNAME            Set DESTNAME to which final package will be renamed (default problem_tag), compression is deduced from extension\n\
  -q, --quiet            Quiet mode\n\
  -t TAG, --tag=TAG      Set problem tag to TAG (cannot be empty)\n\
  -tl VAL, --time-limit=VAL\n\
                         Set time limit VAL in usec on every test, disables automatic time limit setting (only if VAL > 0)\n\
  -uc, --use-conf        If package has valid conf.cfg, use it (disables automatic time limit setting unless one or more option of -g, -vo or -tl is set)\n\
  -v, --verbose          Verbose mode\n\
  -vo, --validate-out    Validate solution output on .out (by checker)\n\
\n\
problem_package tree:\n\
   main/                 Root package folder\n\
   |-- doc/              Documents folder - holds problem statement, elaboration, ... (optional)\n\
   |-- check/            Checker folder - holds checker (optional)\n\
   |-- in/               Tests folder - holds tests (optional)\n\
   |-- prog/             Solutions folder - holds solutions (optional but without solutions automatic time limit setting will be disabled)\n\
   |-- out/              Tests folder - holds tests (optional)\n\
   |-- tests/            Tests folder - holds tests (optional)\n\
   `-- conf.cfg          sim_problem_package config file - holds package config (optional)\n\
\n\
sim_problem_package tree:\n\
   main/                 Root package folder\n\
   |-- doc/              Documents folder - holds problem statement, elaboration, ...\n\
   |-- check/            Checker folder - holds checker\n\
   |-- prog/             Solutions folder - holds solutions\n\
   |-- tests/            Tests folder - holds tests\n\
   `-- conf.cfg          sim_problem_package config file - holds package config\n");
}

/**
 * Pareses options passed to Conver via arguments
 * @param argc like in main (will be modified to hold number of no-arguments)
 * @param argv like in main (holds arguments)
 */
static void parseOptions(int &argc, char **argv) {
	int new_argc = 1;
	long long tmp;

	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == '-') {
			// Force auto limit
			if (0 == strcmp(argv[i], "-fal") ||
					0 == strcmp(argv[i], "--force-auto-limit")) {
				FORCE_AUTO_LIMIT = true;
				TIME_LIMIT = 0;
			}

			// Generate out
			else if (0 == strcmp(argv[i], "-g") ||
					0 == strcmp(argv[i], "--gen-out") ||
					0 == strcmp(argv[i], "--generate-out"))
				GEN_OUT = true;

			// Help
			else if (0 == strcmp(argv[i], "-h") ||
					0 == strcmp(argv[i], "--help")) {
				help();
				exit(0);
			}

			// Memory limit
			else if ((0 == strcmp(argv[i], "-m") ||
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
			}

			// Max time limit
			else if ((0 == strcmp(argv[i], "-mt") ||
					0 == strcmp(argv[i], "--max-time-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp))
					eprintf("Wrong max time limit\n");
				else
					HARD_TIME_LIMIT = tmp;

			} else if (isPrefix(argv[i], "--max-time-limit=")) {
				if (1 > sscanf(argv[i] + 17, "%llu", &tmp))
					eprintf("Wrong max time limit\n");
				else
					HARD_TIME_LIMIT = tmp;
			}

			// Problem name
			else if ((0 == strcmp(argv[i], "-n") ||
					0 == strcmp(argv[i], "--name")) && i + 1 < argc)
				PROBLEM_NAME = argv[++i];

			else if (isPrefix(argv[i], "--name="))
				PROBLEM_NAME = string(argv[i]).substr(7);

			// Destname
			else if (0 == strcmp(argv[i], "-o") && i + 1 < argc)
				DEST_NAME = argv[++i];

			// Quiet mode
			else if (0 == strcmp(argv[i], "-q") ||
					0 == strcmp(argv[i], "--quiet")) {
				QUIET = true;
				VERBOSE = false;
			}

			// Tag
			else if ((0 == strcmp(argv[i], "-t") ||
					0 == strcmp(argv[i], "--tag")) && i + 1 < argc)
				PROBLEM_TAG = argv[++i];

			else if (isPrefix(argv[i], "--tag="))
				PROBLEM_TAG = string(argv[i]).substr(6);

			// Time limit
			else if ((0 == strcmp(argv[i], "-tl") ||
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
			}

			// Verbose mode
			else if (0 == strcmp(argv[i], "-v") ||
					0 == strcmp(argv[i], "--verbose")) {
				QUIET = false;
				VERBOSE = true;
			}

			// Use conf.cfg
			else if (0 == strcmp(argv[i], "-uc") ||
					0 == strcmp(argv[i], "--use-conf"))
				USE_CONF = true;

			// Validate out
			else if (0 == strcmp(argv[i], "-vo") ||
					0 == strcmp(argv[i], "--validate-out"))
				VALIDATE_OUT = true;

			// Unknown
			else
				eprintf("Unknown option: '%s'\n", argv[i]);

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
}

/**
 * @brief Extract package
 * @details Extract package from @p source to @p dest
 *
 * @param source directory or archive (.zip, .7z, .tag.gz, .tgz)
 * @param dest directory to which extracted package will be placed
 * @param stat stat on source
 */
static void extractPackage(const string& source, const string& dest,
		struct stat sb) {

	mkdir(dest);

	if (S_ISDIR(sb.st_mode)) { // Directory
		if (VERBOSE)
			printf("Copying package...\n");

		// If copy error
		if (copy_r(source.c_str(), (dest + "package/").c_str()) == -1) {
			eprintf("Copy error: %s\n", strerror(errno));
			exit(3);
		}

		if (VERBOSE)
			printf("Completed successfully.\n");

	} else if (S_ISREG(sb.st_mode)) { // File
		string extension = getExtension(source);
		vector<string> args;
		E("extension: '%s'\n", extension.c_str());

		// Detect compression type (based on extension)
		// zip
		if (extension == "zip")
			append(args)("unzip")(VERBOSE ? "-o" : "-oq")(source)("-d")
				(dest);
		// 7zip
		else if (extension == "7z")
			append(args)("7z")("x")("-y")(source)("-o" + dest);
		// tar.gz
		else if (extension == "tgz" || isSuffix(source, ".tar.gz"))
			append(args)("tar")(VERBOSE ? "xvzf" : "xzf")(source)("-C")
				(dest);
		// Unknown
		else {
			eprintf("Error: Unsupported file format\n");
			exit(4);
		}

		if (VERBOSE)
			printf("Unpacking package...\n");

		int exit_code = spawn(args[0], args);
		if (exit_code != 0) {
			eprintf("Unpacking error: %s ", args[0].c_str());

			if (exit_code == -1)
				eprintf("Failed to execute\n");
			else if (WIFSIGNALED(exit_code))
				eprintf("killed by signal %i - %s\n", WTERMSIG(exit_code),
					strsignal(WTERMSIG(exit_code)));
			else
				eprintf("returned %i\n", WIFEXITED(exit_code) ?
					WEXITSTATUS(exit_code) : exit_code);

			exit(4);
		}

		if (VERBOSE)
			printf("Completed successfully.\n");

	} else { // Unknown
		eprintf("Error: '%s' - unknown type of file\n", source.c_str());
		exit(5);
	}
}

/**
 * @brief Makes tag from @p str
 * @details Tag is lower of 3 first not blank characters from @p str
 *
 * @param str string to make tag from it
 * @return tag
 */
static string getTag(const string& str) {
	string tag;
	for (size_t i = 0, len = str.size(); tag.size() < 3 && i < len; ++i)
		if (!isblank(str[i]))
			tag += tolower(str[i]);

	if (tag.empty())
		tag = "tag";

	return tag;
}

int main(int argc, char *argv[]) {
	parseOptions(argc, argv);

	if(argc < 2) {
		help();
		return 1;
	}

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	string in_package = argv[1];
	if (VERBOSE)
		printf("in_package: %s\n", in_package.c_str());

	// Check in_package
	struct stat sb;
	if (stat(in_package.c_str(), &sb) == -1) {
		eprintf("Error: stat(%s) - %s\n", in_package.c_str(), strerror(errno));
		return 2;
	}

	tmp_dir.reset(new TemporaryDirectory("/tmp/conver.XXXXXX"));

	// Get current working directory
	string old_working_dir;
	{
		char *buff;
		buff = get_current_dir_name();
		if (NULL == buff || strlen(buff) == 0) {
			eprintf("Error: get_current_dir_name() - %s\n", strerror(errno));
			return -1;
		}

		old_working_dir = buff;
		if (*--old_working_dir.end() != '/')
			old_working_dir += '/';

		free(buff);
	}
	E("CWD: '%s'\n", old_working_dir.c_str());

	// Change current working directory to tmp_dir
	if (chdir(tmp_dir->name()) == -1) {
		eprintf("Error: chdir() - %s\n", strerror(errno));
		return -1;
	}

	// Update in_package - make absolute path
	if (in_package[0] != '/')
		in_package = old_working_dir + in_package;

	// Root package directory (in tmp_dir, before conversion)
	string tmp_package = "root/";
	E("tmp_package: %s\n", tmp_package.c_str());

	// Extract
	extractPackage(in_package, tmp_package, sb);

	// Validate
	tmp_package = validatePackage(tmp_package);

	// Update config
	if (PROBLEM_NAME.size())
		conf_cfg.name = PROBLEM_NAME;
	else if (!USE_CONF)
		conf_cfg.name = "Unnamed";

	if (!USE_CONF)
		conf_cfg.tag = getTag(conf_cfg.name);

	if (SET_MEMORY_LIMIT || !USE_CONF)
		conf_cfg.memory_limit = MEMORY_LIMIT;

	// Convert (out_package - after conversion)
	string out_package = conf_cfg.tag + "/";
	E("out_package: %s\n", out_package.c_str());
	if (convertPackage(tmp_package, out_package) != 0)
		return 7;

	// If no checker set, set default checker
	if (conf_cfg.checker.empty()) {
		putFileContents((out_package + "check/checker.c").c_str(),
			(const char*)default_checker_c, default_checker_c_len);
		conf_cfg.checker = "checker.c";
	}

	// Set limits
	if ((!USE_CONF || GEN_OUT || TIME_LIMIT > 0 || VALIDATE_OUT ||
				FORCE_AUTO_LIMIT) &&
			setLimits(out_package) != 0)
		return 8;

	// Write config
	putFileContents(out_package + "conf.cfg", conf_cfg.dump());

	if (DEST_NAME.empty())
		DEST_NAME = conf_cfg.tag;

	// Update DEST_NAME - make absolute
	if (DEST_NAME[0] != '/')
		DEST_NAME = old_working_dir + DEST_NAME;

	// Detect compression type (based on extension)
	string extension = getExtension(DEST_NAME);
	vector<string> args;

	// zip
	if (extension == "zip")
		append(args)("zip")(VERBOSE ? "-r" : "-rq")(DEST_NAME)
			(out_package);

	// 7zip
	else if (extension == "7z")
		append(args)("7z")("a")("-y")("-mx9")(DEST_NAME)
			(out_package);

	// tar.gz
	else if (extension == "tgz" || isSuffix(DEST_NAME, ".tar.gz"))
		append(args)("tar")(VERBOSE ? "cvzf" : "czf")(DEST_NAME)
			(out_package);

	// Directory
	else {
		if (move(out_package, DEST_NAME) != 0) {
			eprintf("Error moving: '%s' -> '%s' - %s\n",
				out_package.c_str(), DEST_NAME.c_str(),
				strerror(errno));
			return 8;
		}

		return 0;
	}

	int exit_code = spawn(args[0], args);
	if (exit_code != 0) {
		eprintf("Error: %s ", args[0].c_str());

		if (exit_code == -1)
			eprintf("Failed to execute\n");
		else if (WIFSIGNALED(exit_code))
			eprintf("killed by signal %i - %s\n", WTERMSIG(exit_code),
				strsignal(WTERMSIG(exit_code)));
		else
			eprintf("returned %i\n", WIFEXITED(exit_code) ?
				WEXITSTATUS(exit_code) : exit_code);

		return 9;
	}

	return 0;
}
