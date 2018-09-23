#include "../../include/filesystem.h"
#include "../../include/parsers.h"
#include "../../include/sim/simfile.h"
#include "../../include/utilities.h"

#include <cmath>

using std::pair;
using std::string;
using std::vector;

namespace sim {

string Simfile::dump() const {
	string res;
	back_insert(res, "name: ", ConfigFile::escapeString(name), "\n"
		"label: ", ConfigFile::escapeString(label), "\n"
		"statement: ", ConfigFile::escapeString(statement), "\n"
		"checker: ", ConfigFile::escapeString(checker), "\n"
		"solutions: [");

	if (solutions.size()) {
		back_insert(res, ConfigFile::escapeString(solutions[0]));
		for (uint i = 1; i < solutions.size(); ++i)
			back_insert(res, ", ", ConfigFile::escapeString(solutions[i]));
	}
	back_insert(res, "]\n");

	// Memory limit
	if (global_mem_limit >= (1 << 10))
		back_insert(res, "memory_limit: ", global_mem_limit >> 20, '\n');

	// Limits
	back_insert(res, "limits: [");
	for (const TestGroup& group : tgroups) {
		if (group.tests.size())
			res += '\n';
		for (const Test& test : group.tests) {
			string line {concat_tostr(test.name, ' ',
				usecToSecStr(test.time_limit, 6))};

			if (test.memory_limit != global_mem_limit)
				back_insert(line, ' ', test.memory_limit >> 20);

			back_insert(res, '\t', ConfigFile::escapeString(line), '\n');
		}
	}
	res += "]\n";

	// Scoring
	if (tgroups.size()) {
		res += "scoring: [\n";
		for (const TestGroup& group : tgroups)
			if (group.tests.size()) {
				auto p = TestNameComparator::split(group.tests[0].name);
				if (p.tid == "ocen")
					p.gid = "0";
				back_insert(res, '\t',
					ConfigFile::escapeString(intentionalUnsafeStringView(
						concat(p.gid, ' ', group.score))), '\n');
			}
		res += "]\n";
	}

	// Tests files
	res += "tests_files: [\n";
	for (const TestGroup& group : tgroups)
		for (const Test& test : group.tests)
			if (test.in.size() && test.out.size())
				back_insert(res, '\t',
					ConfigFile::escapeString(intentionalUnsafeStringView(
						concat(test.name, ' ', test.in, ' ', test.out))), '\n');

	res += "]\n";

	return res;
}

// Macros because of string concentration in compile-time (in C++11 it is hard
// to achieve in the other way and this macros are pretty more readable than
// some template meta-programming code that concentrates string literals. Also,
// the macros are used only locally, so after all, they are not so evil...)
#define CHECK_IF_ARR(var, name) if (!var.isArray() && var.isSet()) \
	throw std::runtime_error{"Simfile: variable `" name "` has to be" \
		" specified as an array"}
#define CHECK_IF_NOT_ARR(var, name) if (var.isArray()) \
	throw std::runtime_error{"Simfile: variable `" name "` cannot be" \
		" specified as an array"}

void Simfile::loadName() {
	auto&& var = config["name"];
	CHECK_IF_NOT_ARR(var, "name");
	if ((name = var.asString()).empty())
		throw std::runtime_error{"Simfile: missing problem's name"};
}

void Simfile::loadLabel() {
	auto&& var = config["label"];
	CHECK_IF_NOT_ARR(var, "label");
	if ((label = var.asString()).empty())
		throw std::runtime_error{"Simfile: missing problem's label"};
}

void Simfile::loadChecker() {
	auto&& var = config["checker"];
	CHECK_IF_NOT_ARR(var, "checker");
	if (var.asString().empty())
		throw std::runtime_error{"Simfile: missing checker"};

	// Secure path, so that it is not going outside the package
	checker = abspath(var.asString()).erase(0, 1); // Erase '/' from the
	                                               // beginning
}

void Simfile::loadStatement() {
	auto&& var = config["statement"];
	CHECK_IF_NOT_ARR(var, "statement");
	if (var.asString().empty())
		throw std::runtime_error{"Simfile: missing statement"};

	// Secure path, so that it is not going outside the package
	statement = abspath(var.asString()).erase(0, 1); // Erase '/' from the
	                                                 // beginning
}

void Simfile::loadSolutions() {
	auto&& var = config["solutions"];
	CHECK_IF_ARR(var, "solutions");
	if (var.asArray().empty())
		throw std::runtime_error{"Simfile: missing solution"};

	solutions.clear();
	solutions.reserve(var.asArray().size());
	for (const string& str : var.asArray())
		// Secure path, so that it is not going outside the package
		solutions.emplace_back(abspath(str).erase(0, 1)); // Erase '/' from the
		                                                  // beginning
}

void Simfile::loadGlobalMemoryLimitOnly() {
	auto&& ml = config["memory_limit"];
	CHECK_IF_NOT_ARR(ml, "memory_limit");
	if (ml.isSet()) {
		auto invalid_mem_limit = [] {
			return std::runtime_error{"Simfile: invalid memory_limit - it has "
				"to be a positive integer"};
		};

		if (!isDigitNotGreaterThan<(std::numeric_limits<
			decltype(global_mem_limit)>::max() >> 20)>(ml.asString()))
		{
			if (!isDigit(ml.asString()))
				throw invalid_mem_limit();

			throw std::runtime_error{"Simfile: too big value of the "
				"`memory_limit`"};
		}

		global_mem_limit =
			ml.asInt<decltype(global_mem_limit)>() << 20; // Convert from MB to
			                                              // bytes
		if (global_mem_limit == 0)
			throw invalid_mem_limit();
	} else
		global_mem_limit = 0;
}

std::tuple<StringView, uint64_t, uint64_t>
Simfile::parseLimitsItem(StringView item) {
	SimpleParser sp(item);
	// Test name
	StringView test_name {sp.extractNextNonEmpty(isspace)};
	// Time limit
	StringView x {sp.extractNextNonEmpty(isspace)};
	if (!isReal(x))
		throw std::runtime_error{concat_tostr("Simfile: invalid time limit"
			" for the test `", test_name, '`')};

	double tl = stod(x.to_string());
	if (tl <= 0)
		throw std::runtime_error{concat_tostr("Simfile: invalid time limit"
			" of the test `", test_name, "` - it has to be grater than 0")};

	uint64_t time_limit = round(tl * 1000000LL);
	if (time_limit == 0)
		throw std::runtime_error{concat_tostr("Simfile: time limit of the"
			" test `", test_name, "` is to small - after rounding it is"
			" equal to 0 microseconds, but it has to be at least"
			" 1 microsecond")};

	// Memory limit
	uint64_t memory_limit = 0;
	sp.removeLeading(isspace);
	if (not sp.empty()) {
		auto invalid_mem_limit = [&] {
			return std::runtime_error{concat_tostr("Simfile: invalid memory "
				"limit for the test `", test_name, "` - it has to be a "
				"positive integer")};
		};

		if (!isDigitNotGreaterThan<(std::numeric_limits<
			decltype(memory_limit)>::max() >> 20)>(sp))
		{
			if (!isDigit(sp))
				throw invalid_mem_limit();

			throw std::runtime_error{concat_tostr("Simfile: too big memory "
				"limit value for the test `", test_name, "`")};
		}

		// Memory limit of the current test is valid
		strtou(sp, memory_limit);
		if (memory_limit == 0)
			throw invalid_mem_limit();
		else
			memory_limit <<= 20; // Convert from MB to bytes
	}

	return {test_name, time_limit, memory_limit};
}

std::tuple<StringView, int64_t>
Simfile::parseScoringItem(StringView item) {
	SimpleParser sp(item);
	StringView gid = sp.extractNextNonEmpty(isspace);
	if (!isDigit(gid))
		throw std::runtime_error{concat_tostr("Simfile: scoring of the invalid"
			" group `", gid, "` - it has to be a positive integer")};

	int64_t score;
	sp.removeLeading(isspace);
	if (strtoi(sp, score) != (int)sp.size()) // TODO: handle overflows!
		throw std::runtime_error{concat_tostr("Simfile: invalid scoring of the"
			" group `", gid, "`: ", sp)};

	return {gid, score};
}

void Simfile::loadTests() {
	// Global memory limit
	loadGlobalMemoryLimitOnly();

	// Now if global_mem_limit == 0 then it is unset

	/* Limits */

	auto&& limits = config["limits"];
	CHECK_IF_ARR(limits, "limits");

	if (!limits.isSet())
		throw std::runtime_error{"Simfile: missing limits array"};

	AVLDictMap<StringView, TestGroup, StrNumCompare> tests_groups;
	// StringView may be used as Key because it will point to a string in
	// limits.asArray() which is a const reference to vector inside the `limits`
	// variable which will be valid as long as config is not changed
	for (const string& str : limits.asArray()) {
		StringView test_name;
		uint64_t time_limit, memory_limit;
		std::tie(test_name, time_limit, memory_limit) = parseLimitsItem(str);
		if (memory_limit == 0) {
			if (not global_mem_limit)
				throw std::runtime_error{concat_tostr("Simfile: missing memory"
					" limit for the test `", test_name, '`')};

			memory_limit = global_mem_limit;
		}

		// Add test to its group
		auto p = TestNameComparator::split(test_name);
		if (p.gid.empty())
			throw std::runtime_error{concat_tostr("Simfile: missing group id in"
				" the name of the test `", test_name, '`')};

		// tid == "ocen" belongs to the same group as tests with gid == "0"
		if (p.tid == "ocen")
			p.gid = "0";

		tests_groups[p.gid].tests.emplace_back(test_name.to_string(),
			time_limit, memory_limit);
	}

	/* Scoring */

	auto&& scoring = config["scoring"];
	CHECK_IF_ARR(scoring, "scoring");
	if (!scoring.isSet()) { // Calculate scoring automatically
		int groups_no = tests_groups.size() - bool(tests_groups.find("0"));
		int total_score = 100;

		tests_groups.for_each([&](auto&& git) {
			if (git.first != "0")
				total_score -= (git.second.score = total_score / groups_no--);
		});

	} else { // Check and implement defined scoring
		AVLDictMap<StringView, int64_t> sm; // (gid, score)
		for (const string& str : scoring.asArray()) {
			StringView gid;
			int64_t score;
			std::tie(gid, score) = parseScoringItem(str);

			if (not tests_groups.find(gid))
				throw std::runtime_error{concat_tostr("Simfile: scoring of the "
					"invalid group `", gid, "` - there is no test belonging to "
					"this group")};

			auto&& it = sm.emplace(gid, 0);
			if (!it.second)
				throw std::runtime_error{concat_tostr("Simfile: redefined"
					" scoring of the group `", gid, '`')};

			it.first->second = score;
		}

		// Assign scoring to each group
		tests_groups.for_each([&](auto&& git) {
			auto it = sm.find(git.first);
			if (not it)
				throw std::runtime_error{concat_tostr("Simfile: missing scoring"
					" of the group `", git.first, '`')};

			git.second.score = it->second;
		});
	}

	// Move limits from tests_groups to tgroups
	tgroups.clear();
	tgroups.reserve(tests_groups.size());
	tests_groups.for_each([&](auto&& git) {
		tgroups.emplace_back(std::move(git.second));
	});

	// Sort tests in groups
	for (auto& group : tgroups)
		sort(group.tests, [](const Test& a, const Test& b) {
			return TestNameComparator()(a.name, b.name);
		});
}
std::tuple<StringView, StringView, StringView>
Simfile::parseTestFilesItem(StringView item) {
	SimpleParser sp(item);
	StringView name = sp.extractNextNonEmpty(isspace);
	StringView input = sp.extractNextNonEmpty(isspace);
	StringView output = sp.extractNextNonEmpty(isspace);

	sp.removeLeading(isspace);
	if (not sp.empty())
		throw std::runtime_error{concat_tostr("Simfile: `tests_files`: invalid"
			" format of entry for test `", name, "` - excess part: ", sp)};

	return {name, input, output};
}

void Simfile::loadTestsFiles() {
	auto&& tests_files = config["tests_files"];
	CHECK_IF_ARR(tests_files, "tests_files");

	if (!tests_files.isSet())
		throw std::runtime_error{"Simfile: missing tests_files array"};

	AVLDictMap<StringView, pair<StringView, StringView>> files; // test =>
	                                                            // (in, out)
	// StringView can be used because it will point to the config variable
	// "tests_files" member, which becomes unchanged
	for (const string& str : tests_files.asArray()) {
		StringView test_name;
		pair<StringView, StringView> p;
		std::tie(test_name, p.first, p.second) = parseTestFilesItem(str);

		auto it = files.emplace(test_name, p);
		if (not it.second)
			throw std::runtime_error{concat_tostr("Simfile: `test_files`: "
				"redefinition of the test `", test_name, '`')};
	}

	// Assign to each test its files
	for (TestGroup& group : tgroups)
		for (Test& test : group.tests) {
			auto it = files.find(test.name);
			if (not it)
				throw std::runtime_error{concat_tostr("Simfile: no files"
					" specified for the test `", test.name, '`')};

			// Secure paths, so that it is not going outside the package
			test.in = abspath(it->second.first).erase(0, 1); // Erase '/' from
			                                                 // the beginning
			test.out = abspath(it->second.second).erase(0, 1); // The same here
		}

	// Superfluous files declarations are ignored - those for tests not from the
	// limits array
}

void Simfile::validateFiles(StringView package_path) const {
	// Checker
	if (checker.size() &&
		not isRegularFile(concat(package_path, '/', checker)))
	{
		throw std::runtime_error{concat_tostr("Simfile: invalid checker file `",
			checker, '`')};
	}

	// Statement
	if (statement.size() &&
		!isRegularFile(concat(package_path, '/', statement)))
	{
		throw std::runtime_error{concat_tostr("Simfile: invalid statement file"
			" `", statement, '`')};
	}

	// Solutions
	for (auto&& str : solutions)
		if (!isRegularFile(concat(package_path, '/', str)))
			throw std::runtime_error{concat_tostr("Simfile: invalid solution"
				" file `", str, '`')};

	// Tests
	for (const TestGroup& group : tgroups)
		for (const Test& test : group.tests) {
			if (!isRegularFile(concat(package_path, '/', test.in)))
				throw std::runtime_error{concat_tostr("Simfile: invalid test"
					" input file `", test.in, '`')};
			if (!isRegularFile(concat(package_path, '/', test.out)))
				throw std::runtime_error{concat_tostr("Simfile: invalid test"
					" output file `", test.out, '`')};
		}
}

} // namespace sim
