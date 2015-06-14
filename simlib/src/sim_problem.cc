#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/sim_problem.h"
#include "../include/string.h"
#include "../include/utility.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

using std::string;
using std::vector;

string ProblemConfig::dump() {
	string res;
	append(res) << name << '\n'
		<< tag << '\n'
		<< statement << '\n'
		<< checker << '\n'
		<< solution << '\n'
		<< toString(memory_limit) << '\n';

	// Tests
	for (vector<Group>::iterator i = test_groups.begin(),
			end = test_groups.end(); i != end; ++i) {
		if (i->tests.empty())
			continue;

		sort(i->tests.begin(), i->tests.end(), TestCmp());
		append(res) << '\n' << i->tests[0].name << ' '
			<< usecToSecStr(i->tests[0].time_limit, 2) << ' '
			<< toString(i->points) << '\n';

		for (vector<Test>::iterator j = ++i->tests.begin(); j != i->tests.end();
				++j)
			append(res) << j->name << ' ' << usecToSecStr(j->time_limit, 2)
				<< '\n';
	}
	return res;
}

int ProblemConfig::loadConfig(string package_path, unsigned verbosity) {
	// Add slash to package_path
	if (package_path.size() > 0 && *--package_path.end() != '/')
		package_path += '/';

	vector<string> f = getFileByLines(package_path + "conf.cfg",
		GFBL_IGNORE_NEW_LINES);

	if (verbosity > 1)
		printf("Validating conf.cfg...\n");

	// Problem name
	if (f.size() < 1) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Missing problem name\n");

		return -1;
	}

	// Problem tag
	if (f.size() < 2) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Missing problem tag\n");

		return -1;
	}

	if (f[1].size() > 4) {
		if (verbosity > 0)
			eprintf(
				"Error: conf.cfg: Problem tag too long (max 4 characters)\n");

		return -1;
	}

	// Statement
	if (f.size() < 3 || f[2].empty()) {
		if (verbosity > 0)
			eprintf("Warning: conf.cfg: Missing statement\n");

	} else if (f[2].find('/') != string::npos ||
			access(package_path + "doc/" + f[2], F_OK) == -1) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Invalid statement: 'doc/%s' - %s\n",
				f[2].c_str(), strerror(errno));

		return -1;
	}

	// Checker
	if (f.size() < 4) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Missing checker\n");

		return -1;
	}

	if (f[3].find('/') != string::npos ||
			access(package_path + "check/" + f[3], F_OK) == -1) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Invalid checker: 'check/%s' - %s\n",
				f[3].c_str(), strerror(errno));

		return -1;
	}

	// Solution
	if (f.size() < 5 || f[4].empty())
		eprintf("Warning: conf.cfg: Missing solution\n");

	else if (f[4].find('/') != string::npos ||
			access(package_path + "prog/" + f[4], F_OK) == -1) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Invalid solution: 'prog/%s' - %s\n",
				f[4].c_str(), strerror(errno));

		return -1;
	}

	// Memory limit (in kB)
	if (f.size() < 6) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Missing memory limit\n");

		return -1;
	}

	if (strtou(f[5], &memory_limit) == -1) {
		if (verbosity > 0)
			eprintf("Error: conf.cfg: Invalid memory limit: '%s'\n",
				f[5].c_str());

		return -1;
	}

	name = f[0];
	tag = f[1];
	statement = f[2];
	checker = f[3];
	solution = f[4];

	test_groups.clear();

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
			if (verbosity > 0)
				eprintf(
					"Error: conf.cfg: Tests - invalid line format (line %i)\n",
					i);

			return -1;
		}

		// Test name
		if (line[0].find('/') != string::npos ||
				access((package_path + "tests/" + line[0] + ".in"), F_OK) == -1) {
			if (verbosity > 0)
				eprintf("Error: conf.cfg: Invalid test: '%s' - %s\n",
					line[0].c_str(), strerror(errno));

			return -1;
		}

		Test test = { line[0], 0 };

		// Time limit
		if (!isReal(line[1])) {
			if (verbosity > 0)
				eprintf("Error: conf.cfg: Invalid time limit: '%s' (line %i)\n",
					line[1].c_str(), i);
			return -1;
		}

		test.time_limit = round(strtod(line[1].c_str(), NULL) * 1000000LL);

		// Points
		if (line.size() == 3) {
			test_groups.push_back(Group());

			if (strtoi(line[2], &test_groups.back().points) == -1) {
				if (verbosity > 0)
					eprintf("Error: conf.cfg: Invalid points for group: '%s' "
					"(line %i)\n", line[2].c_str(), i);
				return -1;
			}
		}

		test_groups.back().tests.push_back(test);
	}

	if (verbosity > 1)
		printf("Validation passed.\n");

	return 0;
}
