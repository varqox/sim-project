#include "convert_package.h"
#include "default_checker_dump.h"
#include "set_limits.h"
#include "validate_package.h"

#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/process.h>
#include <simlib/utility.h>

using std::string;
using std::unique_ptr;
using std::vector;

// Global variables
unique_ptr<TemporaryDirectory> tmp_dir;
bool GEN_OUT = false;
bool VALIDATE_OUT = false;
bool USE_CONFIG = true;
bool FORCE_AUTO_LIMIT = false;
unsigned VERBOSITY = 1; // 0 - quiet, 1 - normal, 2 or more - verbose
unsigned long long MEMORY_LIMIT = 0; // in KiB
unsigned long long HARD_TIME_LIMIT = 10 * 1000000; // 10 s
unsigned long long TIME_LIMIT = 0; // Not set (in usec)
string PROOT_PATH = "proot"; // Search for PRoot in system
unique_ptr<directory_tree::Node> package_tree_root;
ProblemConfig config_conf;

static bool SET_MEMORY_LIMIT = false; // true if set in options
static string PROBLEM_NAME, DEST_NAME, PROBLEM_TAG;

/**
 * @brief Displays help
 */
static void help(const char* program_name) {
	if (program_name == nullptr)
		program_name = "conver";

	printf("Usage: %s [options] problem_package\n", program_name);
	puts("Convert problem_package to sim_problem_package");
	puts("  problem_package have to be archive (.zip, .tar.gz, .tgz, .7z) or directory");
	puts("");
	puts("Options:");
	puts("  -fal, --force-auto-limit");
	puts("                         Force automatic time limit setting");
	puts("  -g, --gen-out, --generate-out");
	puts("                         Generate .out files (tests), if exist override, disables -vo");
	puts("  -h, --help             Display this information");
	puts("  -ic, --ignore-config   Ignore config.conf (enables automatic time limit setting)");
	puts("  -m MEM_LIMIT, --memory-limit=MEM_LIMIT");
	puts("                         Set problem memory limit MEM_LIMIT in KiB");
	puts("  -mt <VAL>, --max-time-limit=<VAL>");
	puts("                         Set hard max time limit VAL in usec");
	puts("  -n NAME, --name=NAME   Set problem name to NAME (cannot be empty)");
	puts("  -o DESTNAME            Set DESTNAME to which final package will be placed (default problem_tag), compression is deduced from extension");
	puts("  -q, --quiet            Quiet mode");
	puts("  -t TAG, --tag=TAG      Set problem tag to TAG (cannot be empty)");
	puts("  -tl VAL, --time-limit=VAL");
	puts("                         Set time limit VAL in usec on every test, disables automatic time limit setting (only if VAL > 0)");
	puts("  -v, --verbose          Verbose mode");
	puts("  -vo, --validate-out    Validate solution output on .out (by checker)");
	puts("");
	puts("problem_package tree:");
	puts("   main/                 Root package folder");
	puts("   |-- doc/              Documents folder - holds problem statement, elaboration, ... (optional)");
	puts("   |-- check/            Checker folder - holds checker (optional)");
	puts("   |-- in/               Tests folder - holds tests (optional)");
	puts("   |-- prog/             Solutions folder - holds solutions (optional but without solutions automatic time limit setting will be disabled)");
	puts("   |-- out/              Tests folder - holds tests (optional)");
	puts("   |-- tests/            Tests folder - holds tests (optional)");
	puts("   `-- config.conf       sim_problem_package config file - holds package config (optional)");
	puts("");
	puts("sim_problem_package tree:");
	puts("   main/                 Root package folder");
	puts("   |-- doc/              Documents folder - holds problem statement, elaboration, ...");
	puts("   |-- check/            Checker folder - holds checker");
	puts("   |-- prog/             Solutions folder - holds solutions");
	puts("   |-- tests/            Tests folder - holds tests");
	puts("   `-- config.conf       sim_problem_package config file - holds package config");
}

// TODO: change sscanf to something else
/**
 * Parses options passed to Conver via arguments
 * @param argc like in main (will be modified to hold the number of non-option
 *   parameters)
 * @param argv like in main (holds arguments)
 */
static void parseOptions(int &argc, char **argv) {
	int new_argc = 1;
	unsigned long long tmp;

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
				help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);
			}

			// Ignore config.conf
			else if (0 == strcmp(argv[i], "-ic") ||
					0 == strcmp(argv[i], "--ignore-config"))
				USE_CONFIG = false;

			// Memory limit
			else if ((0 == strcmp(argv[i], "-m") ||
					0 == strcmp(argv[i], "--memory-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp)) {
					eprintf("Wrong memory limit\n");
					exit(-1);
				} else {
					MEMORY_LIMIT = tmp;
					SET_MEMORY_LIMIT = true;
				}

			} else if (isPrefix(argv[i], "--memory-limit=")) {
				if (1 > sscanf(argv[i] + 15, "%llu", &tmp)) {
					eprintf("Wrong memory limit\n");
					exit(-1);
				} else {
					MEMORY_LIMIT = tmp;
					SET_MEMORY_LIMIT = true;
				}
			}

			// Max time limit
			else if ((0 == strcmp(argv[i], "-mt") ||
					0 == strcmp(argv[i], "--max-time-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp)) {
					eprintf("Wrong max time limit\n");
					exit(-1);
				} else
					HARD_TIME_LIMIT = tmp;

			} else if (isPrefix(argv[i], "--max-time-limit=")) {
				if (1 > sscanf(argv[i] + 17, "%llu", &tmp)) {
					eprintf("Wrong max time limit\n");
					exit(-1);
				} else
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
					0 == strcmp(argv[i], "--quiet"))
				VERBOSITY = 0;

			// Tag
			else if ((0 == strcmp(argv[i], "-t") ||
					0 == strcmp(argv[i], "--tag")) && i + 1 < argc) {
				PROBLEM_TAG = argv[++i];
				if (PROBLEM_TAG.size() > 4) {
					eprintf("Problem tag too long (max 4 characters)\n");
					exit(-1);
				}
			}

			else if (isPrefix(argv[i], "--tag="))
				PROBLEM_TAG = string(argv[i]).substr(6);

			// Time limit
			else if ((0 == strcmp(argv[i], "-tl") ||
					0 == strcmp(argv[i], "--time-limit")) && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%llu", &tmp)) {
					eprintf("Wrong time limit\n");
					exit(-1);
				} else
					TIME_LIMIT = tmp;

			} else if (isPrefix(argv[i], "--time-limit=")) {
				if (1 > sscanf(argv[i] + 17, "%llu", &tmp)) {
					eprintf("Wrong time limit\n");
					exit(-1);
				} else
					TIME_LIMIT = tmp;
			}

			// Verbose mode
			else if (0 == strcmp(argv[i], "-v") ||
					0 == strcmp(argv[i], "--verbose"))
				VERBOSITY = 2;

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
		const struct stat* sb) {

	if (mkdir_r(dest) == -1) {
		eprintf("Error: cannot create directory: mkdir() - %s\n",
			strerror(errno));
		exit(3);
	}

	if (S_ISDIR(sb->st_mode)) { // Directory
		if (VERBOSITY > 1)
			printf("Copying package...\n");

		// If copy error
		if (copy_r(source.c_str(), (dest + "package/").c_str()) == -1) {
			eprintf("Copy error: %s\n", strerror(errno));
			exit(3);
		}

		if (VERBOSITY > 1)
			printf("Completed successfully.\n");

	} else if (S_ISREG(sb->st_mode)) { // File
		string extension = getExtension(source);
		vector<string> args;
		E("extension: '%s'\n", extension.c_str());

		// Detect compression type (based on extension)
		// zip
		if (extension == "zip")
			append(args)("unzip")(VERBOSITY > 1 ? "-o" : "-oq")(source)("-d")
				(dest);
		// 7zip
		else if (extension == "7z")
			append(args)("7z")("x")("-y")(source)("-o" + dest);
		// tar.gz
		else if (extension == "tgz" || isSuffix(source, ".tar.gz"))
			append(args)("tar")(VERBOSITY > 1 ? "xvzf" : "xzf")(source)("-C")
				(dest);
		// Unknown
		else {
			eprintf("Error: Unsupported file format\n");
			exit(4);
		}

		if (VERBOSITY > 1)
			printf("Unpacking package...\n");

		// Unpack package
		Spawner::ExitStat es;
		try {
			es = Spawner::run(args[0], args, {-1});
		} catch (const std::exception& e) {
			eprintf("Spawner error: %s\n", e.what());
			exit(4);
		}

		if (es.code != 0) {
			eprintf("Unpacking error: %s ", args[0].c_str());
			eprintf("%s\n", es.message.c_str());
			exit(4);
		}

		if (VERBOSITY > 1)
			printf("Completed successfully in %.5lfs.\n", es.runtime / 1e6);

	} else { // Unknown
		eprintf("Error: '%s' - unknown type of file\n", source.c_str());
		exit(5);
	}
}

int main(int argc, char **argv) {
	parseOptions(argc, argv);

	if(argc != 2) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	// Problem name and memory limit are essential
	if (!USE_CONFIG && (PROBLEM_NAME.empty() || !SET_MEMORY_LIMIT)) {
		eprintf("Error: Package without config.conf requires problem name and "
			"memory limit (See options -n and -m)\n");
		return 1;
	}

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);

	string in_package = argv[1];
	if (VERBOSITY > 1)
		printf("in_package: %s\n", in_package.c_str());

	// Stat in_package
	struct stat sb;
	if (stat(in_package.c_str(), &sb) == -1) {
		eprintf("Error: stat(%s) - %s\n", in_package.c_str(), strerror(errno));
		return 2;
	}

	try {
		// Create tmp_dir
		tmp_dir.reset(new TemporaryDirectory("/tmp/conver.XXXXXX"));

	} catch (const std::exception& e) {
		eprintf("Error: Caught exception: %s:%d - %s", __FILE__, __LINE__,
			e.what());
		return 1;
	}

	string old_working_dir;
	try {
		// Get current working directory
		old_working_dir = getCWD();
		E("CWD: '%s'\n", old_working_dir.c_str());

	} catch(const std::exception& e) {
		eprintf("%s\n", e.what());
		return -1;
	}

	// If PRoot is available locally, change PROOT_PATH
	if (access("proot", F_OK) == 0)
		PROOT_PATH = old_working_dir + "proot";

	// Change current working directory to tmp_dir
	if (chdir(tmp_dir->name()) == -1) {
		eprintf("Error: chdir()%s\n", error(errno).c_str());
		return -1;
	}

	// Update in_package - make absolute
	if (in_package[0] != '/')
		in_package = old_working_dir + in_package;

	// Root package directory (in tmp_dir, before conversion)
	string tmp_package = "root/";
	E("tmp_package: %s\n", tmp_package.c_str());

	// Extract
	extractPackage(in_package, tmp_package, &sb);

	// Validate
	tmp_package = validatePackage(tmp_package);

	// Problem name
	if (PROBLEM_NAME.size())
		config_conf.name = PROBLEM_NAME;

	// Problem tag
	if (PROBLEM_TAG.size())
		config_conf.tag = PROBLEM_TAG;
	else if (!USE_CONFIG)
		config_conf.tag = makeTag(config_conf.name);

	// Memory limit
	if (SET_MEMORY_LIMIT)
		config_conf.memory_limit = MEMORY_LIMIT;

	// Problem name and memory limit are essential
	if (config_conf.name.empty() || config_conf.memory_limit == 0) {
		bool both = config_conf.name.empty() && config_conf.memory_limit == 0;
		eprintf("Error: Package requires problem name and memory limit "
			"specified, but there %s no %s neither in config.conf nor in "
			"options (See options -n and -m)\n", (both ? "are" : "is"), (both
				? "problem name and memory limit" : (config_conf.name.empty()
					? "problem name" : "memory limit")));
		return -1;
	}

	// Convert package (out_package - path to package after conversion)
	string out_package = config_conf.tag + "/";
	E("out_package: %s\n", out_package.c_str());
	if (convertPackage(tmp_package, out_package) != 0)
		return 7;

	// If no checker is set, set default checker
	if (config_conf.checker.empty()) {
		config_conf.checker = "checker.c";
		if (putFileContents((out_package + "check/checker.c").c_str(),
				(const char*)default_checker_c,default_checker_c_len) == -1) {
			eprintf("Error: putFileContents()\n");
			return -1;
		}
	}

	// Set limits
	if ((!USE_CONFIG || GEN_OUT || TIME_LIMIT > 0 || VALIDATE_OUT ||
				FORCE_AUTO_LIMIT) &&
			setLimits(out_package) != 0)
		return 8;

	// Write config
	if (putFileContents(concat(out_package, "config.conf"), config_conf.dump())
			== -1) {
		eprintf("Error: putFileContents()\n");
		return -1;
	}

	if (DEST_NAME.empty())
		DEST_NAME = config_conf.tag;

	// Update DEST_NAME - make absolute
	if (DEST_NAME[0] != '/')
		DEST_NAME = old_working_dir + DEST_NAME;

	// Detect compression type (based on extension) of DEST_NAME
	string extension = getExtension(DEST_NAME);
	vector<string> args;

	// zip
	if (extension == "zip")
		append(args)("zip")(VERBOSITY > 1 ? "-r" : "-rq")(DEST_NAME)
			(out_package);

	// 7zip
	else if (extension == "7z")
		append(args)("7z")("a")("-y")("-m0=lzma2")(DEST_NAME)
			(out_package);

	// tar.gz
	else if (extension == "tgz" || isSuffix(DEST_NAME, ".tar.gz"))
		append(args)("tar")(VERBOSITY > 1 ? "cvzf" : "czf")(DEST_NAME)
			(out_package);

	// Directory
	else {
		if (move(out_package, DEST_NAME) != 0) {
			eprintf("Error moving: '%s' -> '%s'%s\n", out_package.c_str(),
				DEST_NAME.c_str(), error(errno).c_str());
			return 8;
		}

		return 0;
	}

	// Compress package
	Spawner::ExitStat es;
	try {
		es = Spawner::run(args[0], args, {-1});
	} catch (const std::exception& e) {
		eprintf("Spawner error: %s\n", e.what());
		return 9;
	}
	if (es.code != 0) {
		eprintf("Error: %s %s\n", args[0].c_str(), es.message.c_str());
		return 9;
	}

	return 0;
}
