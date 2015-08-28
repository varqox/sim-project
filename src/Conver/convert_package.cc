#include "convert_package.h"

#include "../simlib/include/debug.h"

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::pair;
using std::string;
using std::vector;

pair<string, string> TestNameCompatator::extractTag(const string& str) {
	size_t end, i = str.size();
	pair<string, string> res;

	// Test id
	while (i > 0 && isalpha(str[i - 1]))
		--i;
	res.second = str.substr(i);

	// Group id
	end = i;
	while (i > 0 && isdigit(str[i - 1]))
		--i;
	res.first = str.substr(i, end - i);

	return res;
}

int convertPackage(string tmp_package, string out_package) {
	if (tmp_package.size() && *--tmp_package.end() != '/')
		tmp_package += '/';

	if (out_package.size() && *--out_package.end() != '/')
		out_package += '/';

	E("in -> '%s'\nout -> '%s'\n", tmp_package.c_str(), out_package.c_str());

	// Create package structure
	mkdir_r(out_package + "check");
	mkdir(out_package + "doc");
	mkdir(out_package + "prog");
	mkdir(out_package + "tests");

	// Copy all suitable files to out_package
	const char solution_extensions[][10] = { ".c", ".cc", ".cpp" };
	const char statement_extensions[][10] = {
		".pdf",
		".html",
		".htm",
		".txt",
		".md"
	};

	// D(package_tree_root->print(stderr);)

	// check/
	if (USE_CONFIG && config_conf.checker.size())
		copy(tmp_package + "check/" + config_conf.checker, out_package + "check/" +
			config_conf.checker);

	directory_tree::node *folder = package_tree_root->dir("check");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, solution_extensions, solution_extensions +
					sizeof(solution_extensions) /
					sizeof(*solution_extensions))) {

				copy(tmp_package + "check/" + *i, out_package + "check/" + *i);

				if (!USE_CONFIG && config_conf.checker.find_last_of('.') >
						i->find_last_of('.'))
					config_conf.checker = *i;
			}

	// doc/
	if (USE_CONFIG && config_conf.statement.size())
		copy(tmp_package + "doc/" + config_conf.statement, out_package + "doc/" +
			config_conf.statement);

	folder = package_tree_root->dir("doc");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, statement_extensions, statement_extensions +
					sizeof(statement_extensions) /
					sizeof(*statement_extensions))) {

				copy(tmp_package + "doc/" + *i, out_package + "doc/" + *i);

				if (!USE_CONFIG && config_conf.statement.find_last_of('.') >
						i->find_last_of('.'))
					config_conf.statement = *i;
			}

	// prog/
	if (USE_CONFIG) // Copy all solutions
		foreach (i, config_conf.solutions)
			copy(tmp_package + "prog/" + *i, out_package + "prog/" + *i);

	folder = package_tree_root->dir("prog");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, solution_extensions, solution_extensions +
					sizeof(solution_extensions) /
					sizeof(*solution_extensions))) {

				copy(tmp_package + "prog/" + *i, out_package + "prog/" + *i);
				if (!USE_CONFIG)
					config_conf.solutions.push_back(*i);

				if (!USE_CONFIG && config_conf.main_solution.find_last_of('.') >
						i->find_last_of('.'))
					config_conf.main_solution = *i;
			}

	if (VERBOSITY > 1) {
		printf("checker: '%s'\nstatement: '%s'\nmain_solution: '%s'\n"
			"solutions: [", config_conf.checker.c_str(),
			config_conf.statement.c_str(), config_conf.main_solution.c_str());
		foreach (i, config_conf.solutions)
			printf((i == config_conf.solutions.begin() ? "'%s'" : ", '%s'"),
				i->c_str());
		printf("]\n");
	}

	// tests/
	if (USE_CONFIG) {
		foreach (i, config_conf.test_groups)
			foreach (test, i->tests) {
				copy(tmp_package + "tests/" + test->name + ".in",
					out_package + "tests/" + test->name + ".in");
				copy(tmp_package + "tests/" + test->name + ".out",
					out_package + "tests/" + test->name + ".out");
			}
		return 0;
	}

	vector<string> tests; // holds test names
	directory_tree::node *folders[] = {
		package_tree_root->dir("in"),
		package_tree_root->dir("out"),
		package_tree_root->dir("tests"),
	};

	// Search for .in and copy them
	for (int i = 0, end = sizeof(folders) / sizeof(*folders); i < end; ++i)
		if (folders[i] != NULL)
			foreach (it, folders[i]->files)
				if (isSuffix(*it, ".in")) {
					copy(tmp_package + folders[i]->name + "/" + *it,
						out_package + "tests/" + *it);
					tests.push_back(it->substr(0, it->size() - 3));
				}

	if (tests.empty()) { // No tests
		config_conf.test_groups.clear();
		return 0;
	}

	sort(tests.begin(), tests.end());

	// Search for .out and copy them (only if appropriate .in exists)
	for (int i = 0, end = sizeof(folders) / sizeof(*folders); i < end; ++i)
		if (folders[i] != NULL)
			foreach (it, folders[i]->files)
				if (isSuffix(*it, ".out") &&
						binary_search(tests.begin(), tests.end(),
							it->substr(0, it->size() - 4))) {
					copy(tmp_package + folders[i]->name + "/" + *it,
						out_package + "tests/" + *it);
				}

	// Add tests to config_conf
	sort(tests.begin(), tests.end(), TestNameCompatator());

	config_conf.test_groups.assign(1, ProblemConfig::Group());
	ProblemConfig::Group *group = &config_conf.test_groups.back();
	pair<string, string> curr_group,
		last_group = TestNameCompatator::extractTag(tests[0]);

	foreach (i, tests) {
		curr_group = TestNameCompatator::extractTag(*i);
		// Next group (it is strongly based on test order - see
		// TestNameComparator)
		if (curr_group.first != last_group.first &&
				!(curr_group.second == "ocen" &&
					(last_group.first == "0" || last_group.second == "ocen"))) {
			config_conf.test_groups.push_back(ProblemConfig::Group());
			group = &config_conf.test_groups.back();
			last_group = curr_group;
		}

		group->tests.push_back((ProblemConfig::Test){ *i, 0 });
	}
	return 0;
}
