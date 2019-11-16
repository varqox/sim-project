#include "../../include/sim/simfile.hh"
#include "../../include/avl_dict.hh"
#include "../../include/filesystem.hh"
#include "../../include/path.hh"
#include "../../include/simple_parser.hh"
#include "../../include/time.hh"
#include "../../include/utilities.hh"

#include <cmath>

using std::pair;
using std::string;
using std::vector;

static void append_scoring_value(string& res, const sim::Simfile& simfile) {
	res += "[\n";
	for (const sim::Simfile::TestGroup& group : simfile.tgroups)
		if (group.tests.size()) {
			auto p =
			   sim::Simfile::TestNameComparator::split(group.tests[0].name);
			if (p.tid == "ocen")
				p.gid = "0";
			back_insert(
			   res, '\t',
			   ConfigFile::escape_string(intentional_unsafe_string_view(
			      concat(p.gid, ' ', group.score))),
			   '\n');
		}
	res += ']';
}

static void append_limits_value(string& res, const sim::Simfile& simfile) {
	back_insert(res, '[');
	for (const sim::Simfile::TestGroup& group : simfile.tgroups) {
		if (group.tests.size())
			res += '\n';
		for (const sim::Simfile::Test& test : group.tests) {
			string line(
			   concat_tostr(test.name, ' ', to_string(test.time_limit)));

			if (not simfile.global_mem_limit.has_value() or
			    test.memory_limit != simfile.global_mem_limit.value()) {
				back_insert(line, ' ', test.memory_limit >> 20);
			}

			back_insert(res, '\t', ConfigFile::escape_string(line), '\n');
		}
	}
	res += ']';
}

namespace sim {

string Simfile::dump() const {
	string res;
	if (name)
		back_insert(res, "name: ", ConfigFile::escape_string(*name), '\n');

	if (label)
		back_insert(res, "label: ", ConfigFile::escape_string(*label), '\n');

	if (interactive)
		back_insert(res, "interactive: ", interactive, '\n');

	if (statement) {
		back_insert(res, "statement: ", ConfigFile::escape_string(*statement),
		            '\n');
	}

	if (checker.has_value()) {
		back_insert(res, "checker: ", ConfigFile::escape_string(*checker),
		            '\n');
	}

	back_insert(res, "solutions: [");
	if (solutions.size()) {
		back_insert(res, ConfigFile::escape_string(solutions[0]));
		for (uint i = 1; i < solutions.size(); ++i)
			back_insert(res, ", ", ConfigFile::escape_string(solutions[i]));
	}
	back_insert(res, "]\n");

	// Memory limit
	if (global_mem_limit.has_value() and
	    global_mem_limit.value() >= (1 << 20)) {
		back_insert(res, "memory_limit: ", global_mem_limit.value() >> 20,
		            '\n');
	}

	// Limits
	back_insert(res, "limits: ");
	append_limits_value(res, *this);
	res += '\n';

	// Scoring
	if (not tgroups.empty()) {
		back_insert(res, "scoring: ");
		append_scoring_value(res, *this);
		res += '\n';
	}

	// Tests files
	res += "tests_files: [\n";
	for (const TestGroup& group : tgroups)
		for (const Test& test : group.tests)
			if (test.in.size() and (interactive or test.out.value().size())) {
				auto item = concat(test.name, ' ', test.in);
				if (not interactive)
					item.append(' ', test.out.value());

				back_insert(res, '\t', ConfigFile::escape_string(item), '\n');
			}

	res += "]\n";

	return res;
}

string Simfile::dump_scoring_value() const {
	string res;
	append_scoring_value(res, *this);
	return res;
}

string Simfile::dump_limits_value() const {
	string res;
	append_limits_value(res, *this);
	return res;
}

// Macros because of string concentration in compile-time (in C++11 it is hard
// to achieve in the other way and this macros are pretty more readable than
// some template meta-programming code that concentrates string literals. Also,
// the macros are used only locally, so after all, they are not so evil...)
#define CHECK_IF_ARR(var, name)                                                \
	if (!var.is_array() && var.is_set()) {                                     \
		throw std::runtime_error {"Simfile: variable `" name "` has to be"     \
		                          " specified as an array"};                   \
	}
#define CHECK_IF_NOT_ARR(var, name)                                            \
	if (var.is_array()) {                                                      \
		throw std::runtime_error {"Simfile: variable `" name "` cannot be"     \
		                          " specified as an array"};                   \
	}

void Simfile::load_name() {
	auto&& var = config["name"];
	CHECK_IF_NOT_ARR(var, "name");
	if (not var.is_set())
		throw std::runtime_error {"Simfile: missing problem's name"};

	name = var.as_string();
}

void Simfile::load_label() {
	auto&& var = config["label"];
	CHECK_IF_NOT_ARR(var, "label");
	if (not var.is_set())
		throw std::runtime_error {"Simfile: missing problem's label"};

	label = var.as_string();
}

void Simfile::load_interactive() {
	auto&& var = config["interactive"];
	CHECK_IF_NOT_ARR(var, "label");
	if (var.as_string().empty())
		interactive = false;
	else
		interactive = var.as_bool();
}

void Simfile::load_checker() {
	load_interactive();

	auto&& var = config["checker"];
	CHECK_IF_NOT_ARR(var, "checker");

	if (not var.is_set() or var.as_string().empty()) {
		if (interactive) {
			throw std::runtime_error("Simfile: interactive problems require"
			                         " checker (checker is not set)");
		}

		checker = std::nullopt; // No checker - default checker should be used
		return;
	}

	// Secure path, so that it is not going outside the package
	checker = path_absolute(var.as_string()).erase(0, 1); // Erase '/' from the
	                                                      // beginning
}

void Simfile::load_statement() {
	auto&& var = config["statement"];
	CHECK_IF_NOT_ARR(var, "statement");
	if (not var.is_set())
		throw std::runtime_error {"Simfile: missing statement"};
	if (var.as_string().empty())
		throw std::runtime_error {"Simfile: statement cannot be empty"};

	// Secure path, so that it is not going outside the package
	statement = path_absolute(var.as_string()).erase(0, 1); // Erase '/' from
	                                                        // the beginning
}

void Simfile::load_solutions() {
	auto&& var = config["solutions"];
	CHECK_IF_ARR(var, "solutions");
	if (var.as_array().empty())
		throw std::runtime_error {"Simfile: missing solution"};

	solutions.clear();
	solutions.reserve(var.as_array().size());
	for (const string& str : var.as_array())
		// Secure path, so that it is not going outside the package
		solutions.emplace_back(
		   path_absolute(str).erase(0, 1)); // Erase '/' from the
		                                    // beginning
}

void Simfile::load_global_memory_limit_only() {
	auto&& ml = config["memory_limit"];
	CHECK_IF_NOT_ARR(ml, "memory_limit");
	if (ml.is_set()) {
		auto invalid_mem_limit = [] {
			return std::runtime_error {"Simfile: invalid memory_limit - it has "
			                           "to be a positive integer"};
		};

		if (!is_digit_not_greater_than<(
		       std::numeric_limits<decltype(
		          global_mem_limit)::value_type>::max() >>
		       20)>(ml.as_string())) {
			if (!is_digit(ml.as_string()))
				throw invalid_mem_limit();

			throw std::runtime_error {
			   "Simfile: too big value of the `memory_limit`"};
		}

		// Convert from MiB to bytes
		auto gml = ml.as<decltype(global_mem_limit)::value_type>().value()
		           << 20;
		if (gml <= 0)
			throw invalid_mem_limit();

		global_mem_limit = gml;

	} else {
		global_mem_limit = std::nullopt;
	}
}

std::tuple<StringView, std::chrono::nanoseconds, std::optional<uint64_t>>
Simfile::parse_limits_item(StringView item) {
	SimpleParser sp(item);
	// Test name
	StringView test_name {sp.extract_next_non_empty(isspace)};
	// Time limit
	StringView x {sp.extract_next_non_empty(isspace)};
	if (!is_real(x))
		throw std::runtime_error {concat_tostr(
		   "Simfile: invalid time limit for the test `", test_name, '`')};

	double tl = stod(x.to_string());
	if (tl <= 0) {
		throw std::runtime_error {
		   concat_tostr("Simfile: invalid time limit of the test `", test_name,
		                "` - it has to be grater than 0")};
	}

	using std::chrono::duration;
	using std::chrono::duration_cast;
	using std::chrono_literals::operator""ns;
	using std::chrono::nanoseconds;

	auto time_limit = duration_cast<nanoseconds>(duration<double>(tl) + 0.5ns);
	if (time_limit == 0ns) {
		throw std::runtime_error {
		   concat_tostr("Simfile: time limit of the test `", test_name,
		                "` is to small - after rounding it is equal to 0 "
		                "nanoseconds, but it has to be at least 1 nanosecond")};
	}

	// Memory limit
	sp.remove_leading(isspace);
	if (sp.empty())
		return {test_name, time_limit, std::nullopt};

	using MemLimitType = decltype(Test::memory_limit);
	constexpr auto max_mem_limit =
	   std::numeric_limits<MemLimitType>::max() >> 20;

	auto memory_limit_opt = str2num<MemLimitType>(sp, 1, max_mem_limit);
	if (not memory_limit_opt) {
		throw std::runtime_error(concat_tostr(
		   "Simfile: invalid memory limit for the test `", test_name,
		   "` - it has to be a positive integer not greater than",
		   max_mem_limit));
	}

	auto memory_limit = *memory_limit_opt << 20; // Convert from MiB to bytes
	return {test_name, time_limit, memory_limit};
}

std::tuple<StringView, int64_t> Simfile::parse_scoring_item(StringView item) {
	SimpleParser sp(item);
	StringView gid = sp.extract_next_non_empty(isspace);
	if (!is_digit(gid)) {
		throw std::runtime_error {
		   concat_tostr("Simfile: scoring of the invalid group `", gid,
		                "` - it has to be a positive integer")};
	}

	sp.remove_leading(isspace);
	auto score = str2num<int64_t>(sp);
	if (not score) {
		throw std::runtime_error {concat_tostr(
		   "Simfile: invalid scoring of the group `", gid, "`: ", sp, " ")};
	}

	return {gid, *score};
}

void Simfile::load_tests() {
	// Global memory limit
	load_global_memory_limit_only();

	// Now if global_mem_limit == 0 then it is unset

	/* Limits */

	auto&& limits = config["limits"];
	CHECK_IF_ARR(limits, "limits");

	if (!limits.is_set())
		throw std::runtime_error {"Simfile: missing limits array"};

	AVLDictMap<StringView, TestGroup, StrNumCompare> tests_groups;
	// StringView may be used as Key because it will point to a string in
	// limits.as_array() which is a const reference to vector inside the
	// `limits` variable which will be valid as long as config is not changed
	for (const string& str : limits.as_array()) {
		StringView test_name;
		std::chrono::nanoseconds time_limit;
		std::optional<uint64_t> memory_limit;
		std::tie(test_name, time_limit, memory_limit) = parse_limits_item(str);
		if (not memory_limit.has_value()) {
			if (not global_mem_limit.has_value()) {
				throw std::runtime_error {
				   concat_tostr("Simfile: missing memory limit for the test `",
				                test_name, '`')};
			}

			memory_limit = global_mem_limit.value();
		}

		// Add test to its group
		auto p = TestNameComparator::split(test_name);
		if (p.gid.empty()) {
			throw std::runtime_error {concat_tostr(
			   "Simfile: missing group id in the name of the test `", test_name,
			   '`')};
		}

		// tid == "ocen" belongs to the same group as tests with gid == "0"
		if (p.tid == "ocen")
			p.gid = "0";

		tests_groups[p.gid].tests.emplace_back(
		   test_name.to_string(), time_limit, memory_limit.value());
	}

	/* Scoring */

	auto&& scoring = config["scoring"];
	CHECK_IF_ARR(scoring, "scoring");
	if (!scoring.is_set()) { // Calculate scoring automatically
		int groups_no = tests_groups.size() - bool(tests_groups.find("0"));
		int total_score = 100;

		tests_groups.for_each([&](auto&& git) {
			if (git.first != "0")
				total_score -= (git.second.score = total_score / groups_no--);
		});

	} else { // Check and implement defined scoring
		AVLDictMap<StringView, int64_t> sm; // (gid, score)
		for (const string& str : scoring.as_array()) {
			StringView gid;
			int64_t score;
			std::tie(gid, score) = parse_scoring_item(str);

			if (not tests_groups.find(gid)) {
				throw std::runtime_error {concat_tostr(
				   "Simfile: scoring of the invalid group `", gid,
				   "` - there is no test belonging to this group")};
			}

			auto&& it = sm.emplace(gid, 0);
			if (!it.second) {
				throw std::runtime_error {concat_tostr(
				   "Simfile: redefined scoring of the group `", gid, '`')};
			}

			it.first->second = score;
		}

		// Assign scoring to each group
		tests_groups.for_each([&](auto&& git) {
			auto it = sm.find(git.first);
			if (not it) {
				throw std::runtime_error {concat_tostr(
				   "Simfile: missing scoring of the group `", git.first, '`')};
			}

			git.second.score = it->second;
		});
	}

	// Move limits from tests_groups to tgroups
	tgroups.clear();
	tgroups.reserve(tests_groups.size());
	tests_groups.for_each(
	   [&](auto&& git) { tgroups.emplace_back(std::move(git.second)); });

	// Sort tests in groups
	for (auto& group : tgroups)
		sort(group.tests, [](const Test& a, const Test& b) {
			return TestNameComparator()(a.name, b.name);
		});
}

std::tuple<StringView, StringView, StringView>
Simfile::parse_test_files_item(StringView item) {
	SimpleParser sp(item);
	StringView name = sp.extract_next_non_empty(isspace);
	StringView input = sp.extract_next_non_empty(isspace);
	StringView output = sp.extract_next_non_empty(isspace);

	sp.remove_leading(isspace);
	if (not sp.empty()) {
		throw std::runtime_error {concat_tostr(
		   "Simfile: `tests_files`: invalid format of entry for test `", name,
		   "` - excess part: ", sp)};
	}

	return {name, input, output};
}

void Simfile::load_tests_files() {
	load_interactive();

	auto&& tests_files = config["tests_files"];
	CHECK_IF_ARR(tests_files, "tests_files");

	if (!tests_files.is_set())
		throw std::runtime_error {"Simfile: missing tests_files array"};

	AVLDictMap<StringView, pair<StringView, StringView>> files; // test =>
	                                                            // (in, out)
	// StringView can be used because it will point to the config variable
	// "tests_files" member, which becomes unchanged
	for (const string& str : tests_files.as_array()) {
		StringView test_name, in_file, out_file;
		std::tie(test_name, in_file, out_file) = parse_test_files_item(str);
		if (test_name.empty())
			continue; // Ignore empty entries

		if (in_file.empty()) {
			throw std::runtime_error(concat_tostr(
			   "Simfile: `test_files`: missing input file for test `",
			   test_name, '`'));
		}

		if (interactive and not out_file.empty()) {
			throw std::runtime_error(concat_tostr(
			   "Simfile: `test_files`: output file specified for test `",
			   test_name,
			   "`, but interactive problems have no test output files"));
		}

		if (not interactive and out_file.empty()) {
			throw std::runtime_error(concat_tostr(
			   "Simfile: `test_files`: missing output file for test: `",
			   test_name, '`'));
		}

		auto it = files.emplace(
		   test_name, pair<StringView, StringView>(in_file, out_file));
		if (not it.second) {
			throw std::runtime_error {
			   concat_tostr("Simfile: `test_files`: redefinition of the test `",
			                test_name, '`')};
		}
	}

	// Assign to each test its files
	for (TestGroup& group : tgroups)
		for (Test& test : group.tests) {
			auto it = files.find(test.name);
			if (not it) {
				throw std::runtime_error {
				   concat_tostr("Simfile: no files specified for the test `",
				                test.name, '`')};
			}

			// Secure paths, so that it is not going outside the package
			test.in =
			   path_absolute(it->second.first).erase(0, 1); // Erase '/' from
			                                                // the beginning
			if (interactive)
				test.out = std::nullopt;
			else
				test.out =
				   path_absolute(it->second.second).erase(0, 1); // Same here
		}

	// Superfluous files declarations are ignored - those for tests not from
	// the limits array
}

void Simfile::validate_files(StringView package_path) const {
	// Checker
	if (checker.has_value() and
	    (checker->empty() or
	     not is_regular_file(concat(package_path, '/', checker.value())))) {
		throw std::runtime_error {concat_tostr(
		   "Simfile: invalid checker file `", checker.value(), '`')};
	}

	// Statement
	if (statement &&
	    !is_regular_file(concat(package_path, '/', statement.value()))) {
		throw std::runtime_error {
		   concat_tostr("Simfile: invalid statement file `", *statement, '`')};
	}

	// Solutions
	for (auto&& str : solutions) {
		if (!is_regular_file(concat(package_path, '/', str))) {
			throw std::runtime_error {
			   concat_tostr("Simfile: invalid solution file `", str, '`')};
		}
	}

	// Tests
	for (const TestGroup& group : tgroups) {
		for (const Test& test : group.tests) {
			if (!is_regular_file(concat(package_path, '/', test.in))) {
				throw std::runtime_error {concat_tostr(
				   "Simfile: invalid test input file `", test.in, '`')};
			}
			if (test.out.has_value() and
			    !is_regular_file(concat(package_path, '/', test.out.value()))) {
				throw std::runtime_error {
				   concat_tostr("Simfile: invalid test output file `",
				                test.out.value(), '`')};
			}
		}
	}
}

} // namespace sim
