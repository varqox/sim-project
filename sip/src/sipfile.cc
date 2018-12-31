#include "sip_error.h"
#include "sipfile.h"
#include "utils.h"

#include <simlib/filesystem.h>
#include <simlib/optional.h>
#include <simlib/sim/judge_worker.h>
#include <simlib/sim/problem_package.h>
#include <simlib/sim/simfile.h>

// Macros because of string concentration in compile-time (in C++11 it is hard
// to achieve in the other way and this macros are pretty more readable than
// some template meta-programming code that concentrates string literals. Also,
// the macros are used only locally, so after all, they are not so evil...)
#define CHECK_IF_ARR(var, name) if (!var.isArray() && var.isSet()) \
	throw SipError("Sipfile: variable `" name "` has to be" \
		" specified as an array")
#define CHECK_IF_NOT_ARR(var, name) if (var.isArray()) \
	throw SipError("Sipfile: variable `" name "` cannot be" \
		" specified as an array")

void Sipfile::loadDefaultTimeLimit() {
	auto&& dtl = config["default_time_limit"];
	CHECK_IF_NOT_ARR(dtl, "default_time_limit");

	if (dtl.asString().empty())
		throw SipError("Sipfile: missing default_time_limit");

	auto x = dtl.asString();
	if (!isReal(x))
		throw SipError("Sipfile: invalid default time limit");

	double tl = stod(x);
	if (tl <= 0)
		throw SipError("Sipfile: default time limit has to be grater than 0");

	default_time_limit = round(tl * 1000000LL);
	if (default_time_limit == 0) {
		throw SipError("Sipfile: default time limit is to small - after"
			" rounding it is equal to 0 microseconds, but it has to be at least"
			" 1 microsecond");
	}
}

template<class Func>
static void for_each_test_in_range(StringView test_range, Func&& callback) {
	STACK_UNWINDING_MARK;

	auto hypen = test_range.find('-');
	if (hypen == StringView::npos) {
		callback(test_range);
		return;
	}

	if (hypen + 1 == test_range.size())
		throw SipError("invalid test range (trailing hypen): ", test_range);

	StringView begin = test_range.substring(0, hypen);
	StringView end = test_range.substring(hypen + 1);

	auto begin_test = sim::Simfile::TestNameComparator::split(begin);
	auto end_test = sim::Simfile::TestNameComparator::split(end);
	// The part before group id
	StringView prefix = begin.substring(0,
		begin.size() - begin_test.gid.size() - begin_test.tid.size());
	{
		// Allow e.g. test4a-5c
		StringView end_prefix = end.substring(0,
			end.size() - end_test.gid.size() - end_test.tid.size());
		if (not end_prefix.empty() and end_prefix != prefix) {
			throw SipError("invalid test range (test prefix of the end test is"
				" not empty and does not match the begin test prefix): ",
				test_range);
		}
	}

	if (not isDigitNotGreaterThan<ULLONG_MAX>(begin_test.gid))
		throw SipError("invalid test range (group id is too big): ", test_range);

	unsigned long long begin_gid = strtoull(begin_test.gid);
	unsigned long long end_gid;

	if (end_test.gid.empty()) { // Allow e.g. 4a-d
		end_gid = begin_gid;
	} else if (not isDigitNotGreaterThan<ULLONG_MAX>(end_test.gid)) {
		throw SipError("invalid test range (group id is too big): ", test_range);
	} else {
		end_gid = strtoull(end_test.gid);
	}

	std::string begin_tid = tolower(begin_test.tid.to_string());
	std::string end_tid = tolower(end_test.tid.to_string());

	// Allow e.g. 4-8c
	if (begin_tid.empty())
		begin_tid = end_tid;

	if (begin_tid.size() != end_tid.size()) {
		throw SipError("invalid test range (test IDs have different length): ",
			test_range);
	}

	if (begin_tid > end_tid) {
		throw SipError("invalid test range (begin test ID is greater than end"
			" test ID): ", test_range);
	}

	// Increments tid by one
	auto inc_tid = [](auto& tid) {
		if (tid.size == 0)
			return;

		++tid.back();
		for (int i = tid.size - 1; i > 0 and tid[i] > 'z'; --i) {
			tid[i] -= ('z' - 'a');
			++tid[i - 1];
		}
	};

	for (auto gid = begin_gid; gid <= end_gid; ++gid)
		for (InplaceBuff<8> tid(begin_tid); tid <= end_tid; inc_tid(tid))
			callback(intentionalUnsafeStringView(concat(prefix, gid, tid)));
}

void Sipfile::loadStaticTests() {
	STACK_UNWINDING_MARK;

	auto static_tests_var = config.getVar("static");
	CHECK_IF_ARR(static_tests_var, "static");

	static_tests.clear();
	for (StringView entry : static_tests_var.asArray()) {
		entry.extractLeading(isspace);
		StringView test_range = entry.extractLeading(not_isspace);
		entry.extractLeading(isspace);
		if (not entry.empty()) {
			log_warning("Sipfile (static): ignoring invalid suffix: `", entry,
				"` of the entry with test range: ", test_range);
		}

		for_each_test_in_range(test_range, [&](StringView test) {
			bool inserted = static_tests.emplace(test);
			if (not inserted) {
				log_warning("Sipfile (static): test `", test, "` is specified"
					" in more than one test range");
			}
		});
	}
}

static InplaceBuff<32> matching_generator(StringView pattern,
	const sim::PackageContents& utils_contents)
{
	STACK_UNWINDING_MARK;

	Optional<StringView> res;
	utils_contents.for_each_with_prefix("", [&](StringView file) {
		if (is_subsequence(pattern, file) and sim::is_source(file)) {
			if (res.has_value()) {
				throw SipError("Sipfile: specified generator `", pattern,
					"` matches more than one file: `", res.value(), "` and `",
					file, '`');
			}

			res = file;
		}
	});

	if (res.has_value())
		return InplaceBuff<32>(*res);

	log_warning("Simfile (gen): no file matches specified generator: `",
		pattern, "`. It will be treated as a shell command; to remove this"
		" warning, prefix the generator with sh: - e.g. sh:echo");
	return concat<32>("sh:", pattern);
}

void Sipfile::loadGenTests() {
	STACK_UNWINDING_MARK;

	auto gen_tests_var = config.getVar("gen");
	CHECK_IF_ARR(gen_tests_var, "gen");

	sim::PackageContents utils_contents;
	utils_contents.load_from_directory("utils/", true);
	utils_contents.remove_with_prefix("utils/cache/");

	gen_tests.clear();
	for (StringView entry : gen_tests_var.asArray()) {
		// Test range
		entry.extractLeading(isspace);
		StringView test_range = entry.extractLeading(not_isspace);
		entry.extractLeading(isspace);
		// Generator
		StringView specified_generator = entry.extractLeading(not_isspace);
		entry.extractLeading(isspace);
		// Generator arguments
		entry.extractTrailing(isspace);
		StringView generator_args = entry;

		if (specified_generator.empty())
			throw SipError("Sipfile (gen): missing generator for the test range"
				" `", test_range, '`');

		// Match generator
		InplaceBuff<32> generator;
		if (hasPrefix(specified_generator, "sh:") or
			access(abspath(specified_generator), F_OK) == 0)
		{
			generator = specified_generator;
		} else {
			generator = matching_generator(specified_generator, utils_contents);
		}

		for_each_test_in_range(test_range, [&](StringView test) {
			bool inserted = gen_tests.emplace(test, generator, generator_args);
			if (not inserted) {
				throw SipError("Sipfile (gen): test `", test, "` is specified"
					" in more than one test range");
			}
		});
	}
}
