#include "validate_package.h"

#include "../include/debug.h"
#include "../include/string.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <vector>

using std::string;
using std::vector;

int validateConf(string package_path) {
	if (package_path.empty()) {
		eprintf("Invalid package path\n");
		return -1;
	}

	if (*--package_path.end() != '/')
		package_path += '/';

	vector<string> f = getFileByLines(package_path + "conf.cfg",
		GFBL_IGNORE_NEW_LINES);

	if (VERBOSITY > 1)
		printf("Validating conf.cfg...\n");

	// Problem name
	if (f.size() < 1) {
		eprintf("Error: conf.cfg: Missing problem name\n");
		return -1;
	}

	// Problem tag
	if (f.size() < 2) {
		eprintf("Error: conf.cfg: Missing problem tag\n");
		return -1;
	}

	if (f[1].size() > 4) {
		eprintf("Error: conf.cfg: Problem tag too long (max 4 characters)\n");
		return -1;
	}

	// Statement
	if (f.size() < 3 || f[2].empty())
		eprintf("Warning: conf.cfg: Missing statement\n");

	else if (f[2].find('/') != string::npos ||
			access(package_path + "doc/" + f[2], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid statement: 'doc/%s' - %s\n",
			f[2].c_str(), strerror(errno));
		return -1;
	}

	// Checker
	if (f.size() < 4) {
		eprintf("Error: conf.cfg: Missing checker\n");
		return -1;
	}

	if (f[3].find('/') != string::npos ||
			access(package_path + "check/" + f[3], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid checker: 'check/%s' - %s\n",
			f[3].c_str(), strerror(errno));
		return -1;
	}

	// Solution
	if (f.size() < 5 || f[4].empty())
		eprintf("Warning: conf.cfg: Missing solution\n");

	else if (f[4].find('/') != string::npos ||
			access(package_path + "prog/" + f[4], F_OK) == -1) {
		eprintf("Error: conf.cfg: Invalid solution: 'prog/%s' - %s\n",
			f[4].c_str(), strerror(errno));
		return -1;
	}

	// Memory limit (in kB)
	if (f.size() < 6) {
		eprintf("Error: conf.cfg: Missing memory limit\n");
		return -1;
	}

	if (strtou(f[5], &conf_cfg.memory_limit) == -1) {
		eprintf("Error: conf.cfg: Invalid memory limit: '%s'\n", f[5].c_str());
		return -1;
	}

	conf_cfg.name = f[0];
	conf_cfg.tag = f[1];
	conf_cfg.statement = f[2];
	conf_cfg.checker = f[3];
	conf_cfg.solution = f[4];

	conf_cfg.test_groups.clear();

	// Tests
	vector<string> line;
	for (int i = 6, flen = f.size(); i < flen; ++i) {
		size_t beg = 0, end = 0, len = f[i].size();
		line.clear();

		// Get line split on spaces and tabs
		do {
			end = beg + strcspn(f[i].c_str() + beg, " \t");
			line.push_back(string(f[i].begin() + beg, f[i].begin() + end));

			// Remove blank strings (created by multiple separators)
			if (line.back().empty())
				line.pop_back();

			beg = ++end;
		} while (end < len);

		// Ignore empty lines
		if (line.empty())
			continue;

		// Validate line
		if (line.size() != 2 && line.size() != 3) {
			eprintf("Error: conf.cfg: Tests - invalid line format (line %i)\n",
				i);
			return -1;
		}

		// Test name
		if (line[0].find('/') != string::npos ||
				access((package_path + "tests/" + line[0] + ".in"), F_OK) == -1) {
			eprintf("Error: conf.cfg: Invalid test: '%s' - %s\n",
				line[0].c_str(), strerror(errno));
			return -1;
		}

		ProblemConfig::Test test = { line[0], 0 };

		// Time limit
		if (!isReal(line[1])) {
			eprintf("Error: conf.cfg: Invalid time limit: '%s' (line %i)\n",
				line[1].c_str(), i);
			return -1;
		}

		test.time_limit = round(strtod(line[1].c_str(), NULL) * 1000000LL);

		// Points
		if (line.size() == 3) {
			conf_cfg.test_groups.push_back(ProblemConfig::Group());

			if (strtoi(line[2], &conf_cfg.test_groups.back().points) == -1) {
				eprintf("Error: conf.cfg: Invalid points for group: '%s' "
					"(line %i)\n", line[2].c_str(), i);
				return -1;
			}
		}

		conf_cfg.test_groups.back().tests.push_back(test);
	}

	if (VERBOSITY > 1)
		printf("Validation passed.\n");

	return 0;
}

string validatePackage(string pathname) {
	DIR *dir = opendir(pathname.c_str()), *subdir;
	dirent *file;

	if (dir == NULL) {
		eprintf("Error: Failed to open directory: '%s' - %s\n",
			pathname.c_str(), strerror(errno));
		abort(); // Should not happen
	}

	if (*--pathname.end() != '/')
		pathname += '/';

	string main_folder;
	while ((file = readdir(dir))) {
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, ".."))
			if ((subdir = opendir((pathname + file->d_name).c_str()))) {
				if (main_folder.size()) {
					eprintf("Error: More than one package main folder found\n");
					exit(6);
				}
				closedir(subdir);
				main_folder = file->d_name;
			}
	}

	if (main_folder.empty()) {
		eprintf("Error: No main folder found in package\n");
		exit(6);
	}

	pathname.append(main_folder);
	if (*--pathname.end() != '/')
		pathname +='/';

	// Get package file structure
	package_tree_root.reset(directory_tree::dumpDirectoryTree(pathname));
	if (package_tree_root.isNull()) {
		eprintf("Failed to get package file structure: %s\n", strerror(errno));
		abort(); // This is probably a bug
	}

	// Checkout conf.cfg
	if (!USE_CONF || !std::binary_search(package_tree_root->files.begin(),
			package_tree_root->files.end(), "conf.cfg") ||
			validateConf(pathname) != 0)
		USE_CONF = false;

	return pathname;
}
