#include "convert_package.h"

#include <simlib/debug.h>

using std::pair;
using std::string;
using std::vector;

pair<string, string> TestNameComparator::extractTag(const string& str) {
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
	if (tmp_package.size() && tmp_package.back() != '/')
		tmp_package += '/';

	if (out_package.size() && out_package.back() != '/')
		out_package += '/';

	E("in -> '%s'\nout -> '%s'\n", tmp_package.c_str(), out_package.c_str());

	// Create package structure
	// TODO: these lines look to similarly
	if (mkdir_r(out_package + "check") == -1) {
		eprintf("Failed to create check/: mkdir_r()%s\n", error(errno).c_str());
		return -1;
	}

	if (mkdir(out_package + "doc") == -1) {
		eprintf("Failed to create doc/: mkdir()%s\n", error(errno).c_str());
		return -1;
	}

	if (mkdir(out_package + "prog") == -1) {
		eprintf("Failed to create prog/: mkdir()%s\n", error(errno).c_str());
		return -1;
	}

	if (mkdir(out_package + "tests") == -1) {
		eprintf("Failed to create tests/: mkdir()%s\n", error(errno).c_str());
		return -1;
	}

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
		copy(concat(tmp_package, "check/", config_conf.checker),
			concat(out_package, "check/", config_conf.checker));

	auto folder = package_tree_root->dir("check");
	if (folder != nullptr)
		for (auto& i : folder->files)
			if (hasSuffixIn(i, solution_extensions, solution_extensions +
				sizeof(solution_extensions) / sizeof(*solution_extensions)))
			{
				copy(concat(tmp_package, "check/", i),
					concat(out_package, "check/", i));

				if (!USE_CONFIG && config_conf.checker.find_last_of('.') >
					i.find_last_of('.'))
				{
					config_conf.checker = i;
				}
			}

	// doc/
	if (USE_CONFIG && config_conf.statement.size())
		copy(concat(tmp_package, "doc/", config_conf.statement),
			concat(out_package, "doc/", config_conf.statement));

	folder = package_tree_root->dir("doc");
	if (folder != nullptr)
		for (auto& i : folder->files)
			if (hasSuffixIn(i, statement_extensions, statement_extensions +
				sizeof(statement_extensions) / sizeof(*statement_extensions)))
			{
				copy(concat(tmp_package, "doc/", i),
					concat(out_package, "doc/", i));

				if (!USE_CONFIG && config_conf.statement.find_last_of('.') >
					i.find_last_of('.'))
				{
					config_conf.statement = i;
				}
			}

	// prog/
	if (USE_CONFIG) // Copy all solutions
		for (auto& i : config_conf.solutions)
			copy(concat(tmp_package, "prog/", i),
				concat(out_package, "prog/", i));

	folder = package_tree_root->dir("prog");
	if (folder != nullptr)
		for (auto& i : folder->files)
			if (hasSuffixIn(i, solution_extensions, solution_extensions +
				sizeof(solution_extensions) / sizeof(*solution_extensions)))
			{
				copy(concat(tmp_package, "prog/", i),
					concat(out_package, "prog/", i));
				if (!USE_CONFIG)
					config_conf.solutions.emplace_back(i);

				if (!USE_CONFIG && config_conf.main_solution.find_last_of('.') >
					i.find_last_of('.'))
				{
					config_conf.main_solution = i;
				}
			}

	if (VERBOSITY > 1) {
		printf("checker: '%s'\nstatement: '%s'\nmain_solution: '%s'\n"
			"solutions: [", config_conf.checker.c_str(),
			config_conf.statement.c_str(), config_conf.main_solution.c_str());

		for (auto i = config_conf.solutions.begin();
			i != config_conf.solutions.end(); ++i)
		{
			printf((i == config_conf.solutions.begin() ? "'%s'" : ", '%s'"),
				i->c_str());
		}

		printf("]\n");
	}

	// tests/
	if (USE_CONFIG) {
		for (auto& i : config_conf.test_groups)
			for (auto& test : i.tests) {
				copy(concat(tmp_package, "tests/", test.name, ".in"),
					concat(out_package, "tests/", test.name, ".in"));
				copy(concat(tmp_package, "tests/", test.name, ".out"),
					concat(out_package, "tests/", test.name, ".out"));
			}
		return 0;
	}

	vector<string> tests; // holds test names
	directory_tree::Node *folders[] = {
		package_tree_root->dir("in"),
		package_tree_root->dir("out"),
		package_tree_root->dir("tests"),
	};

	// Search for .in and copy them
	for (auto& dir : folders)
		if (dir != nullptr)
			for (auto& file : dir->files)
				if (hasSuffix(file, ".in")) {
					copy(concat(tmp_package, dir->name, '/', file),
						concat(out_package, "tests/", file));
					tests.emplace_back(file.substr(0, file.size() - 3));
				}

	if (tests.empty()) { // No tests
		config_conf.test_groups.clear();
		return 0;
	}

	sort(tests.begin(), tests.end());

	// Search for .out and copy them (only if appropriate .in exists)
	for (auto& dir : folders)
		if (dir != nullptr)
			for (auto& file : dir->files)
				if (hasSuffix(file, ".out") && binary_search(tests.begin(),
					tests.end(), file.substr(0, file.size() - 4)))
				{
					copy(concat(tmp_package, dir->name, '/', file),
						concat(out_package, "tests/", file));
				}

	/* Add tests to config_conf */
	sort(tests.begin(), tests.end(), TestNameComparator());

	config_conf.test_groups.assign(1, sim::Simfile::Group());
	sim::Simfile::Group *group = &config_conf.test_groups.back();
	pair<string, string> curr_group,
		last_group = TestNameComparator::extractTag(tests[0]);

	for (auto& i : tests) {
		curr_group = TestNameComparator::extractTag(i);
		// Next group (it is strongly based on the test order - see
		// TestNameComparator)
		if (curr_group.first != last_group.first &&
			!(curr_group.second == "ocen" &&
				(last_group.first == "0" || last_group.second == "ocen")))
		{
			config_conf.test_groups.emplace_back(); // Add new group
			group = &config_conf.test_groups.back();
			last_group = curr_group;
		}

		group->tests.emplace_back(i, 0);
	}
	return 0;
}
