#include "../../include/config_file.h"
#include "../../include/debug.h"
#include "../../include/filesystem.h"
#include "../../include/logger.h"
#include "../../include/sim/simfile.h"
#include "../../include/utilities.h"

#include <cmath>

using std::string;
using std::vector;

namespace sim {

string Simfile::dump() const {
	string res;
	back_insert(res, "name: ", ConfigFile::safeString(name), '\n',
		"tag: ", ConfigFile::safeString(tag), '\n',
		"statement: ", ConfigFile::safeString(statement), '\n',
		"checker: ", ConfigFile::safeString(checker), '\n',
		"memory_limit: ", toString(memory_limit), '\n',
		"main_solution: ", ConfigFile::safeString(main_solution), '\n');

	// Solutions
	res += "solutions: [";
	for (auto i = solutions.begin(); i != solutions.end(); ++i)
		back_insert(res, (i == solutions.begin() ? "" : ", "),
			ConfigFile::safeString(*i));
	res += "]\n";

	// Tests
	res += "tests: [";
	bool first_test = true;
	for (auto& i : test_groups) {
		if (i.tests.empty())
			continue;

		if (first_test)
			first_test = false;
		else
			res += ",\n";

		back_insert(res, "\n\t'", i.tests[0].name, ' ',
			usecToSecStr(i.tests[0].time_limit, 2), ' ', toString(i.points),
			'\'');

		for (auto j = ++i.tests.begin(); j != i.tests.end(); ++j)
			back_insert(res, ",\n\t'", j->name, ' ', usecToSecStr(j->time_limit,
				2), '\'');
	}
	res += "\n]\n";
	return res;
}

vector<string> Simfile::loadFrom(string package_path) noexcept(false) {
	// Append slash to package_path
	if (package_path.size() && package_path.back() != '/')
		package_path += '/';

	ConfigFile config;
	config.addVars("name", "tag", "statement", "checker", "memory_limit",
		"solutions", "main_solution", "tests");

	config.loadConfigFromFile(package_path + "config.conf");

	vector<string> warnings;
	// Problem name
	name = config.getString("name");
	if (name.empty())
		warnings.emplace_back("config.conf: missing problem name");

	// Memory limit (in KiB)
	if (!config.isSet("memory_limit")) {
		memory_limit = 0;
		warnings.emplace_back("config.conf: missing memory limit\n");

	} else if (strtou(config.getString("memory_limit"), &memory_limit) <= 0)
		warnings.emplace_back("config.conf: invalid memory limit\n");

	// Problem tag
	tag = config.getString("tag");
	if (tag.size() > 4) // Invalid tag
		warnings.emplace_back("conf.cfg: problem tag too long "
			"(max 4 characters)");

	// Statement
	statement = config.getString("statement");
	if (statement.size()) {
		if (statement.find('/') < statement.size())
			warnings.emplace_back("config.conf: statement cannot contain "
				"'/' character");

		if (access(concat(package_path, "doc/", statement), F_OK) == -1)
			THROW("config.conf: invalid statement '", statement, "': 'doc/",
				statement, '\'', error(errno));
	}

	// Checker
	checker = config.getString("checker");
	if (checker.size() && checker.find('/') < checker.size())
		warnings.emplace_back("config.conf: checker cannot contain "
			"'/' character");

	// Solutions
	solutions = config.getArray("solutions");
	for (auto& i : solutions)
		if (i.find('/') < i.size())
			warnings.emplace_back("config.conf: solution cannot contain "
				"'/' character");

	// Main solution
	main_solution = config.getString("main_solution");
	if (main_solution.empty())
		warnings.emplace_back("config.conf: missing main_solution");

	else if (std::find(solutions.begin(), solutions.end(), main_solution) ==
		solutions.end())
	{
		warnings.emplace_back("config.conf: main_solution has to be set in "
			"solutions");
	}

	// Tests
	test_groups.clear();
	vector<string> tests = config.getArray("tests");
	for (auto& i : tests) {
		Test test;

		// Test name
		size_t pos = 0;
		while (pos < i.size() && !isspace(i[pos]))
			++pos;
		test.name.assign(i, 0, pos);

		if (test.name.find('/') != string::npos)
			warnings.emplace_back("config.conf: test name cannot contain "
				"'/' character");

		// Time limit
		errno = 0;
		char *ptr;
		test.time_limit = round(strtod(i.data() + pos, &ptr) * 1000000LL);
		if (errno)
			warnings.emplace_back(concat("config.conf: ", test.name,
				": invalid time limit"));
		pos = ptr - i.data();

		while (pos < i.size() && isspace(i[pos]))
			++pos;

		// Points
		if (pos < i.size()) {
			test_groups.emplace_back();

			// Remove trailing white spaces
			size_t end = i.size();
			while (end > pos && isspace(i[end - 1]))
				--end;

			if (strtoi(i, &test_groups.back().points, pos, end) <= 0)
				warnings.emplace_back(concat("config.conf: ", test.name,
					": invalid points"));
		}

		test_groups.back().tests.emplace_back(test);
	}

	return warnings;
}

void Simfile::loadFromAndValidate(string package_path) noexcept(false) {
	// Append slash to package_path
	if (package_path.size() && package_path.back() != '/')
		package_path += '/';

	ConfigFile config;
	config.addVars("name", "tag", "statement", "checker", "memory_limit",
		"solutions", "main_solution", "tests");

	config.loadConfigFromFile(package_path + "config.conf");

	// Problem name
	name = config.getString("name");
	if (name.empty())
		THROW("config.conf: Missing problem name");

	// Memory limit (in KiB)
	if (!config.isSet("memory_limit"))
		THROW("config.conf: missing memory limit\n");

	if (strtou(config.getString("memory_limit"), &memory_limit) <= 0)
		THROW("config.conf: invalid memory limit\n");

	// Problem tag
	tag = config.getString("tag");
	if (tag.size() > 4) // Invalid tag
		THROW("conf.cfg: Problem tag too long (max 4 characters)");

	// Statement
	statement = config.getString("statement");
	if (statement.size()) {
		if (statement.find('/') < statement.size())
			THROW("config.conf: statement cannot contain '/' character");

		if (access(concat(package_path, "doc/", statement), F_OK) == -1)
			THROW("config.conf: invalid statement '", statement, "': 'doc/",
				statement, '\'', error(errno));
	}

	// Checker
	checker = config.getString("checker");
	if (checker.size()) {
		if (checker.find('/') < checker.size())
			THROW("config.conf: checker cannot contain '/' character");

		if (access(concat(package_path, "check/", checker), F_OK) == -1)
			THROW("config.conf: invalid checker '", checker, "': 'check/",
				checker, '\'', error(errno));
	}

	// Solutions
	solutions = config.getArray("solutions");
	for (auto& i : solutions) {
		if (i.find('/') < i.size())
			THROW("config.conf: solution cannot contain '/' character");

		if (access(concat(package_path, "prog/", i), F_OK) == -1)
			THROW("config.conf: invalid solution '", i, "': 'prog/", i, '\'',
				error(errno));
	}

	// Main solution
	main_solution = config.getString("main_solution");
	if (main_solution.empty())
		THROW("config.conf: missing main_solution");

	if (std::find(solutions.begin(), solutions.end(), main_solution) ==
		solutions.end())
	{
		THROW("config.conf: main_solution has to be set in solutions");
	}

	// Tests
	test_groups.clear();
	vector<string> tests = config.getArray("tests");
	for (auto& i : tests) {
		Test test;

		// Test name
		size_t pos = 0;
		while (pos < i.size() && !isspace(i[pos]))
			++pos;
		test.name.assign(i, 0, pos);

		if (test.name.find('/') != string::npos)
			THROW("config.conf: test name cannot contain '/' character");

		string test_in = concat(package_path, "tests/", test.name, ".in");
		if (access(test_in, F_OK) == -1)
			THROW("config.conf: invalid test name '", test_in, '\'',
				error(errno));

		// Time limit
		errno = 0;
		char *ptr;
		test.time_limit = round(strtod(i.data() + pos, &ptr) * 1000000LL);
		if (errno)
			THROW("config.conf: ", test.name, ": invalid time limit");
		pos = ptr - i.data();

		while (pos < i.size() && isspace(i[pos]))
			++pos;

		// Points
		if (pos < i.size()) {
			test_groups.emplace_back();

			// Remove trailing white spaces
			size_t end = i.size();
			while (end > pos && isspace(i[end - 1]))
				--end;

			if (strtoi(i, &test_groups.back().points, pos, end) <= 0)
				THROW("config.conf: ", test.name, ": invalid points");
		}

		test_groups.back().tests.emplace_back(test);
	}
}

string makeTag(const string& str) {
	string tag;
	for (size_t i = 0, len = str.size(); tag.size() < 3 && i < len; ++i)
		if (!isblank(str[i]))
			tag += tolower(str[i]);

	return tag;
}

string obtainCheckerOutput(int fd, size_t max_length) noexcept(false) {
	string res(max_length, '\0');

	(void)lseek(fd, 0, SEEK_SET);
	size_t pos = 0;
	ssize_t k;
	do {
		k = read(fd, const_cast<char*>(res.data()) + pos, max_length - pos);
		if (k > 0) {
			pos += k;
		} else if (k == 0) {
			// We have read whole checker output
			res.resize(pos);
			// Remove trailing white characters
			while (isspace(res.back()))
				res.pop_back();
			return res;

		} else if (errno != EINTR)
			THROW("read()", error(errno));

	} while (pos < max_length);

	// Checker output is longer than max_length

	// Replace last 3 characters with "..."
	if (max_length >= 3)
		res[max_length - 3] = res[max_length - 2] = res[max_length - 1] = '.';

	return res;
}

} // namespace sim
