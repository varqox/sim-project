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

	D(package_tree_root->print(stderr);)

	// check/
	if (USE_CONF && conf_cfg.checker.size())
		copy(tmp_package + "check/" + conf_cfg.checker, out_package + "check/" +
			conf_cfg.checker);

	directory_tree::node *folder = package_tree_root->dir("check");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, solution_extensions, solution_extensions +
					sizeof(solution_extensions) /
					sizeof(*solution_extensions))) {

				copy(tmp_package + "check/" + *i, out_package + "check/" + *i);

				if (!USE_CONF && conf_cfg.checker.find_last_of('.') >
						i->find_last_of('.'))
					conf_cfg.checker = *i;
			}

	// doc/
	if (USE_CONF && conf_cfg.statement.size())
		copy(tmp_package + "doc/" + conf_cfg.statement, out_package + "doc/" +
			conf_cfg.statement);

	folder = package_tree_root->dir("doc");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, statement_extensions, statement_extensions +
					sizeof(statement_extensions) /
					sizeof(*statement_extensions))) {

				copy(tmp_package + "doc/" + *i, out_package + "doc/" + *i);

				if (!USE_CONF && conf_cfg.statement.find_last_of('.') >
						i->find_last_of('.'))
					conf_cfg.statement = *i;
			}

	// prog/
	if (USE_CONF && conf_cfg.solution.size())
		copy(tmp_package + "prog/" + conf_cfg.solution, out_package + "prog/" +
			conf_cfg.solution);

	folder = package_tree_root->dir("prog");
	if (folder != NULL)
		foreach (i, folder->files)
			if (isSuffixIn(*i, solution_extensions, solution_extensions +
					sizeof(solution_extensions) /
					sizeof(*solution_extensions))) {

				copy(tmp_package + "prog/" + *i, out_package + "prog/" + *i);

				if (!USE_CONF && conf_cfg.solution.find_last_of('.') >
						i->find_last_of('.'))
					conf_cfg.solution = *i;
			}

	if (VERBOSITY > 1)
		printf("checker: '%s'\nstatement: '%s'\nsolution: '%s'\n",
			conf_cfg.checker.c_str(), conf_cfg.statement.c_str(),
			conf_cfg.solution.c_str());

	// tests/
	if (USE_CONF) {
		foreach (i, conf_cfg.test_groups)
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

	// Add tests to conf_cfg
	sort(tests.begin(), tests.end(), TestNameCompatator());

	conf_cfg.test_groups.assign(1, ProblemConfig::Group());
	ProblemConfig::Group *group = &conf_cfg.test_groups.back();
	string last_group = TestNameCompatator::extractTag(tests[0]).first;
	pair<string, string> curr_tag;

	foreach (i, tests) {
		curr_tag = TestNameCompatator::extractTag(*i);

		if (curr_tag.first != last_group) {
			conf_cfg.test_groups.push_back(ProblemConfig::Group());
			group = &conf_cfg.test_groups.back();
			last_group = curr_tag.first;
		}

		group->tests.push_back((ProblemConfig::Test){ *i, 0 });
	}
	return 0;
}
