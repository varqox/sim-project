#include "default_checker_dump.h"
#include "sip.h"

#include "simlib/include/filesystem.h"

using std::string;
using std::vector;

namespace command {

int init(int argc, char **argv) noexcept(false) {
	if (argc > 3) {
		puts("Usage: sip init [directory] [name]");
		puts("Initialize Sip package in [directory] (by default current directory) and (if specified) set problem name to [name]");
		return 2;
	}

	string directory = (argc < 2 || argv[1][0] == '\0' ? "./" : argv[1]);
	if (directory.back() != '/')
		directory += '/';

	// Create directory structure of the package
	verbose_log(2, "Creating directories...");
	if ((mkdir_r(directory) == -1 && errno != EEXIST)
			|| (mkdir(directory + "check") == -1 && errno != EEXIST)
			|| (mkdir(directory + "doc") == -1 && errno != EEXIST)
			|| (mkdir(directory + "in") == -1 && errno != EEXIST)
			|| (mkdir(directory + "out") == -1 && errno != EEXIST)
			|| (mkdir(directory + "prog") == -1 && errno != EEXIST)
			|| (mkdir(directory + "utils") == -1 && errno != EEXIST))
		throw std::runtime_error(concat("mkdir()", error(errno)));

	bool overwrite_config = true;
	// Load config
	if (access(directory + "config.conf", F_OK) == 0) {
		overwrite_config = false;
		verbose_log(2, "Loading config (loosely)...");

		vector<string> warnings = pconf.loadFrom(directory);
		for (string const& warning : warnings)
			verbose_log(1, "\e[1;35mWarning:\e[m ", warning);
	}

	// Add checker if there is no one
	if (pconf.checker.empty()) {
		overwrite_config = true;
		pconf.checker = "checker.c";
		pconf.memory_limit = 65536; // 8 MB

		verbose_log(2, "Adding default checker...");
		if (putFileContents((directory + "check/checker.c").c_str(),
				(const char*)default_checker_c,
				default_checker_c_len) == -1)
			throw std::runtime_error(concat("putFileContents()", error(errno)));
	}

	// Name
	if (argc > 2) {
		pconf.name = argv[2];
		overwrite_config = true;
	}

	verbose_log(2, "Writing config...");
	if (overwrite_config && putFileContents(directory + "config.conf",
			pconf.dump()) == -1)
		throw std::runtime_error(concat("putFileContents()", error(errno)));

	verbose_log(1, "Done.");
	return 0;
}

int set(int argc, char **argv) noexcept(false) {
	if (argc < 3) {
		puts("Usage: sip set <name> <value>");
		puts("Set <name> to <value> (e.g. 'sip set tag sum' sets problem tag to sum)");
		return 2;
	}

	verbose_log(2, "Loading config (loosely)...");
	vector<string> warnings = pconf.loadFrom(".");
	for (string const& warning : warnings)
		verbose_log(1, "\e[1;35mWarning:\e[m ", warning);

	bool config_changed = false;
	if (strcmp(argv[1], "name") == 0) {
		pconf.name = argv[2];
		config_changed = true;

	} else if (strcmp(argv[1], "tag") == 0) {
		pconf.tag = argv[2];
		config_changed = true;

	} else if (strcmp(argv[1], "statement") == 0) {
		if (access(concat("doc/", argv[2]), F_OK) == -1) {
			errlog("\e[1;31mError:\e[m Statement: '", argv[2], "' does not "
				"exit");
			return 2;
		}

		pconf.statement = argv[2];
		config_changed = true;

	} else if (strcmp(argv[1], "checker") == 0) {
		if (access(concat("check/", argv[2]), F_OK) == -1) {
			errlog("\e[1;31mError:\e[m Checker: '", argv[2], "' does not "
				"exit");
			return 2;
		}

		pconf.checker = argv[2];
		config_changed = true;

	} else if (strcmp(argv[1], "memory_limit") == 0) {
		if (strtoi(argv[2], &pconf.memory_limit) < 1) {
			errlog("\e[1;31mError:\e[m Wrong memory limit: '", argv[2], "'");
			return 2;
		}
		config_changed = true;

	} else if (strcmp(argv[1], "main_solution") == 0) {
		if (access(concat("prog/", argv[2]), F_OK) == -1) {
			errlog("\e[1;31mError:\e[m Solution: '", argv[2], "' does not "
				"exit");
			return 2;
		}

		pconf.main_solution = argv[2];
		config_changed = true;
		if (std::find(pconf.solutions.begin(), pconf.solutions.end(), argv[2])
				== pconf.solutions.end()) {
			pconf.solutions.emplace_back(argv[2]);
			verbose_log(1, "Added ", argv[2], " to solutions.");
		}

	} else {
		errlog("Unknown name: '", argv[1], "'");
		return 2;
	}

	if (config_changed && putFileContents("config.conf", pconf.dump()) == -1)
		throw std::runtime_error(concat("putFileContents()", error(errno)));

	verbose_log(1, "Set ", argv[1], " to '", argv[2], "' -> \e[1;32mDone.\e[m");
	return 0;
}

} // namespace command
