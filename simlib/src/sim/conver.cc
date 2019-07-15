#include "../../include/sim/conver.h"
#include "../../include/debug.h"
#include "../../include/libzip.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/sim/problem_package.h"
#include "../../include/spawner.h"
#include "../../include/utilities.h"

using std::pair;
using std::string;
using std::vector;

namespace sim {

Conver::ConstructionResult Conver::construct_simfile(const Options& opts,
                                                     bool be_verbose) {
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (opts.memory_limit.has_value() and opts.memory_limit.value() <= 0)
		THROW("If set, memory_limit has to be greater than 0");
	if (opts.global_time_limit.has_value() and
	    opts.global_time_limit.value() <= 0ns)
		THROW("If set, global_time_limit has to be greater than 0");
	if (opts.max_time_limit <= 0ns)
		THROW("max_time_limit has to be greater than 0");
	if (opts.rtl_opts.min_time_limit < 0ns)
		THROW("min_time_limit has to be non-negative");
	if (opts.rtl_opts.solution_runtime_coefficient < 0)
		THROW("solution_runtime_coefficient has to be non-negative");

	// Reset report_
	report_.str.clear();
	report_.log_to_stdlog_ = be_verbose;

	/* Load contents of the package */

	PackageContents pc;
	if (isDirectory(package_path_)) {
		throw_assert(package_path_.size() > 0);
		if (package_path_.back() != '/')
			package_path_ += '/';
		// From now on, package_path_ has trailing '/' iff it is a directory
		pc.load_from_directory(package_path_);

	} else
		pc.load_from_zip(package_path_);

	StringView master_dir = pc.master_dir(); // Find master directory if exists

	auto exists_in_pkg = [&](StringView file) {
		return (not file.empty() and pc.exists(intentionalUnsafeStringView(
		                                concat(master_dir, file))));
	};

	// "utils/" directory is ignored by Conver
	pc.remove_with_prefix(
	   intentionalUnsafeStringView(concat(master_dir, "utils/")));

	// Load the Simfile from the package
	Simfile sf;
	if (not opts.ignore_simfile and exists_in_pkg("Simfile")) {
		try {
			sf = Simfile([&] {
				if (package_path_.back() == '/')
					return getFileContents(
					   concat(package_path_, master_dir, "Simfile"));

				ZipFile zip(package_path_, ZIP_RDONLY);
				return zip.extract_to_str(
				   zip.get_index(concat(master_dir, "Simfile")));
			}());
		} catch (const ConfigFile::ParseError& e) {
			THROW(concat_tostr("(Simfile) ", e.what(), '\n', e.diagnostics()));
		}
	}

	// Name
	if (opts.name.size()) {
		sf.name = opts.name;
		report_.append("Problem's name (specified in options): ", sf.name);
	} else {
		report_.append("Problem's name is not specified in options - loading it"
		               " from Simfile");
		sf.load_name();
		report_.append("  name loaded from Simfile: ", sf.name);
	}

	// Label
	if (opts.label.size()) {
		sf.label = opts.label;
		report_.append("Problem's label (specified in options): ", sf.label);
	} else {
		report_.append("Problem's label is not specified in options - loading"
		               " it from Simfile");
		try {
			sf.load_label();
			report_.append("  label loaded from Simfile: ", sf.label);

		} catch (const std::exception& e) {
			report_.append("  ", e.what(),
			               " -> generating label from the"
			               " problem's name");

			sf.label = shorten_name(sf.name);
			report_.append("  label generated from name: ", sf.label);
		}
	}

	auto collect_files = [&pc](auto&& prefix, auto&& cond) {
		vector<StringView> res;
		pc.for_each_with_prefix(prefix, [&](StringView file) {
			if (cond(file))
				res.emplace_back(file);
		});
		return res;
	};

	// Checker
	try {
		sf.load_checker();
		if (not sf.checker.has_value()) {
			report_.append("Missing checker in the package's Simfile -"
			               " searching for one");
		} else if (not exists_in_pkg(sf.checker.value())) {
			report_.append("Invalid checker specified in the package's Simfile"
			               " - searching for one");
			sf.checker = std::nullopt;
		}
	} catch (...) {
		report_.append("Invalid checker specified in the package's Simfile -"
		               " searching for one");
	}

	// Interactive
	if (opts.interactive.has_value())
		sf.interactive = opts.interactive.value();

	if (not sf.checker.has_value()) {
		// Scan check/ and checker/ directory
		auto x = collect_files(concat(master_dir, "check/"), is_source);
		{
			auto y = collect_files(concat(master_dir, "checker/"), is_source);
			x.insert(x.end(), y.begin(), y.end());
		}
		if (x.size()) {
			// Choose the one with the shortest path to be the checker
			std::swap(x.front(), *std::min_element(
			                        x.begin(), x.end(), [](auto&& a, auto&& b) {
				                        return a.size() < b.size();
			                        }));

			sf.checker = x.front().substr(master_dir.size()).to_string();
			report_.append("Chosen checker: ", sf.checker.value());

		} else if (sf.interactive) {
			report_.append("No checker was found");
			THROW("No checker was found, but interactive problems require "
			      "checker");
		} else {
			report_.append(
			   "No checker was found - the default checker will be used");
		}
	}

	// Exclude check/ and checker/ directories from future searches
	pc.remove_with_prefix(
	   intentionalUnsafeStringView(concat(master_dir, "check/")));
	pc.remove_with_prefix(
	   intentionalUnsafeStringView(concat(master_dir, "checker/")));

	// Statement
	try {
		sf.load_statement();
	} catch (...) {
	}

	if (not exists_in_pkg(sf.statement)) {
		report_.append("Missing / invalid statement specified in the package's"
		               " Simfile - searching for one");

		auto is_statement = [](StringView file) {
			return hasSuffixIn(file, {".pdf", ".html", ".htm", ".md", ".txt"});
		};

		// Scan doc/ directory
		auto x = collect_files(concat(master_dir, "doc/"), is_statement);
		if (x.empty())
			x = collect_files("", is_statement); // Scan whole directory tree

		// If there is no statement
		if (x.empty()) {
			if (opts.require_statement)
				THROW("No statement was found");
			else
				report_.append(
				   "\033[1;35mwarning\033[m: no statement was found");

		} else {
			// Prefer PDF to other formats, then the shorter paths
			// The ones with shorter paths are preferred
			std::swap(
			   x.front(),
			   *std::min_element(x.begin(), x.end(), [](auto&& a, auto&& b) {
				   return (
				      pair<bool, size_t>(not hasSuffix(a, ".pdf"), a.size()) <
				      pair<bool, size_t>(not hasSuffix(b, ".pdf"), b.size()));
			   }));

			sf.statement = x.front().substr(master_dir.size()).to_string();
			report_.append("Chosen statement: ", sf.statement);
		}
	}

	// Solutions
	try {
		sf.load_solutions();
	} catch (...) {
	}
	{
		vector<std::string> solutions;
		AVLDictSet<StringView> solutions_set; // Used to detect and eliminate
		                                      // solutions duplication
		for (auto&& sol : sf.solutions) {
			if (exists_in_pkg(sol) and not solutions_set.find(sol)) {
				solutions.emplace_back(sol);
				solutions_set.emplace(sol);
			} else
				report_.append("\033[1;35mwarning\033[m: ignoring invalid"
				               " solution: `",
				               sol, '`');
		}

		auto x = collect_files("", is_source);
		if (solutions.empty()) { // The main solution has to be set
			if (x.empty())
				THROW("No solution was found");

			// Choose the one with the shortest path to be the model solution
			std::swap(x.front(), *std::min_element(
			                        x.begin(), x.end(), [](auto&& a, auto&& b) {
				                        return a.size() < b.size();
			                        }));
		}

		// Merge solutions
		for (auto&& s : x) {
			s.removePrefix(master_dir.size());
			if (solutions_set.emplace(s))
				solutions.emplace_back(s.to_string());
		}

		// Put the solutions into the Simfile
		sf.solutions = std::move(solutions);
	}

	struct Test {
		StringView name;
		std::optional<StringView> in;
		std::optional<StringView> out;
		std::optional<std::chrono::nanoseconds> time_limit;
		std::optional<uint64_t> memory_limit;
	};

	AVLDictMap<StringView, Test> tests;

	// Load tests and limits form variable "limits"
	auto const& limits = sf.config["limits"];
	if (not opts.ignore_simfile and limits.is_set() and limits.is_array())
		for (auto const& str : limits.as_array()) {
			Test test;
			try {
				std::tie(test.name, test.time_limit, test.memory_limit) =
				   Simfile::parse_limits_item(str);
			} catch (const std::exception& e) {
				report_.append("\033[1;35mwarning\033[m: \"limits\": ignoring"
				               " unparsable item -> ",
				               e.what());
				continue;
			}

			if (Simfile::TestNameComparator::split(test.name).gid.empty()) {
				report_.append("\033[1;35mwarning\033[m: \"limits\": ignoring"
				               " test `",
				               test.name,
				               "` because it has no group id in its name");
				continue;
			}

			tests.emplace(test.name,
			              std::move(test)); // Replace on redefinition
		}

	if (opts.seek_for_new_tests)
		pc.for_each_with_prefix("", [&](StringView entry) {
			if (hasSuffix(entry, ".in") or
			    (not sf.interactive and hasSuffix(entry, ".out"))) {
				entry.removeTrailing([](char c) { return (c != '.'); });
				entry.removeSuffix(1);
				StringView name =
				   entry.extractTrailing([](char c) { return (c != '/'); });

				tests[name].name = name;
			}
		});

	// Match tests with the test files found in the package
	pc.for_each_with_prefix("", [&](StringView input) {
		if (hasSuffix(input, ".in")) {
			StringView tname = input;
			tname.removeSuffix(3);
			tname = tname.extractTrailing([](char c) { return (c != '/'); });
			input.removePrefix(master_dir.size());

			auto& test = tests[tname];
			if (test.in.has_value()) {
				report_.append(
				   "\033[1;35mwarning\033[m: input file for test `", tname,
				   "` was found in more than one location: `", test.in.value(),
				   "` and `", input, "` - choosing the later");
			}

			test.in = input;
		}
	});

	if (not sf.interactive) {
		pc.for_each_with_prefix("", [&](StringView output) {
			if (hasSuffix(output, ".out")) {
				StringView tname = output;
				tname.removeSuffix(4);
				tname =
				   tname.extractTrailing([](char c) { return (c != '/'); });
				output.removePrefix(master_dir.size());

				auto& test = tests[tname];
				if (test.out.has_value()) {
					report_.append("\033[1;35mwarning\033[m: output file for"
					               " test `",
					               tname,
					               "` was found in more than one"
					               " location: `",
					               test.out.value(), "` and `", output,
					               "`"
					               " - choosing the later");
				}

				test.out = output;
			}
		});
	}

	// Load test files (this one may overwrite the files form previous step)
	auto const& tests_files = sf.config["tests_files"];
	if (not opts.ignore_simfile and tests_files.is_set() and
	    tests_files.is_array())
		for (auto const& str : tests_files.as_array()) {
			StringView tname;
			std::optional<StringView> input;
			std::optional<StringView> output;
			try {
				std::tie(tname, input, output) =
				   Simfile::parse_test_files_item(str);
			} catch (const std::exception& e) {
				report_.append("\033[1;35mwarning\033[m: \"tests_files\":"
				               " ignoring unparsable item -> ",
				               e.what());
				continue;
			}

			if (tname.empty())
				continue; // Ignore empty entries

			if (input.value().empty()) {
				report_.append("\033[1;35mwarning\033[m: \"tests_files\":"
				               " missing test input file for test `",
				               tname, "` - ignoring");

				throw_assert(output.value().empty());
				continue; // Nothing more to do
			}

			auto path = concat(master_dir, input.value());
			if (not pc.exists(path)) {
				report_.append("\033[1;35mwarning\033[m: \"tests_files\": test"
				               " input file: `",
				               input.value(), "` not found - ignoring file");
				input = std::nullopt;
			}

			path = concat(master_dir, output.value());
			if (sf.interactive) {
				if (not output.value().empty()) {
					report_.append(
					   "\033[1;35mwarning\033[m: \"tests_files\":"
					   " test output file: `",
					   input.value(),
					   "` specified,"
					   " but the problem is interactive - ignoring file");
				}
				output = std::nullopt;

			} else if (output.value().empty()) {
				report_.append("\033[1;35mwarning\033[m: \"tests_files\":"
				               " missing test output file for test `",
				               tname, "` - ignoring");
				output = std::nullopt;

			} else if (not pc.exists(path)) {
				report_.append("\033[1;35mwarning\033[m: \"tests_files\": test"
				               " output file: `",
				               output.value(), "` not found - ignoring file");
				output = std::nullopt;
			}

			if (opts.seek_for_new_tests)
				tests[tname].name = tname; // Add test if it does not exist

			auto it = tests.find(tname);
			if (not it) {
				report_.append(
				   "\033[1;35mwarning\033[m: \"tests_files\": ignoring files "
				   "for test `",
				   tname, "` because no such test is specified in \"limits\"");
				continue;
			}

			Test& test = it->second;
			if (input.has_value())
				test.in = input;

			if (output.has_value())
				test.out = output;
		}

	// Remove tests that have at most one file set
	for (auto it = tests.front(); it; it = tests.upper_bound(it->first)) {
		auto const& test = it->second;
		// Warn if the test was loaded from "limits"
		if (not test.in.has_value() and test.time_limit.has_value())
			report_.append("\033[1;35mwarning\033[m: limits: ignoring test `",
			               test.name,
			               "` because it has no corresponding input file");
		// Warn if the test was loaded from "limits"
		if (not sf.interactive and not test.out.has_value() and
		    test.time_limit.has_value()) {
			report_.append("\033[1;35mwarning\033[m: limits: ignoring test `",
			               test.name,
			               "` because it has no corresponding output file");
		}

		if (not test.in.has_value() or
		    (not sf.interactive and not test.out.has_value()))
			tests.erase(test.name);
	}

	if (not opts.memory_limit.has_value()) {
		if (opts.ignore_simfile)
			THROW("Missing memory limit - global memory limit not specified in"
			      " options and simfile is ignored");

		report_.append("Global memory limit not specified in options - loading"
		               " it from Simfile");

		sf.load_global_memory_limit_only();
		if (not sf.global_mem_limit.has_value()) {
			report_.append("Memory limit is not specified in the Simfile");
			report_.append(
			   "\033[1;35mwarning\033[m: no global memory limit is set");
		}
	}

	// Update the memory limits
	if (opts.memory_limit.has_value()) {
		// Convert from MiB to bytes
		sf.global_mem_limit = opts.memory_limit.value() << 20;
		tests.for_each([&](auto& keyval) {
			keyval.second.memory_limit = sf.global_mem_limit;
		});
	} else { // Give tests without specified memory limit the global memory
		     // limit
		tests.for_each([&](auto& keyval) {
			if (not keyval.second.memory_limit.has_value()) {
				if (not sf.global_mem_limit.has_value())
					THROW("Memory limit is not specified for test `",
					      keyval.first, "` and no global memory limit is set");

				keyval.second.memory_limit = sf.global_mem_limit;
			}
		});
	}

	struct TestsGroup {
		StringView name;
		std::optional<int64_t> score;
		vector<Test> tests;
	};

	AVLDictMap<StringView, TestsGroup, StrNumCompare> tests_groups;
	// Fill tests_groups
	tests.for_each([&](auto& keyval) {
		Test& test = keyval.second;
		auto sr = Simfile::TestNameComparator::split(test.name);
		if (sr.gid.empty())
			return; // Ignore the tests with no group id

		if (sr.tid == "ocen")
			sr.gid = "0";

		auto& group = tests_groups[sr.gid];
		group.name = sr.gid;
		group.tests.emplace_back(std::move(test));
	});

	// Load scoring
	auto const& scoring = sf.config["scoring"];
	if (not opts.ignore_simfile and not opts.reset_scoring and
	    scoring.is_set() and scoring.is_array()) {
		for (auto const& str : scoring.as_array()) {
			StringView gid;
			int64_t score;
			try {
				std::tie(gid, score) = Simfile::parse_scoring_item(str);
			} catch (const std::exception& e) {
				report_.append("\033[1;35mwarning\033[m: \"scoring\": ignoring"
				               " unparsable item -> ",
				               e.what());
				continue;
			}

			auto it = tests_groups.find(gid);
			if (it)
				it->second.score = score;
			else {
				report_.append("\033[1;35mwarning\033[m: \"scoring\": ignoring"
				               " score of the group with id `",
				               gid, "` because no test belongs to it");
			}
		}
	}

	// Sum positive scores
	uint64_t max_score = 0;
	size_t unscored_tests = 0;
	tests_groups.for_each([&](auto const& keyval) {
		max_score += meta::max(keyval.second.score.value_or(0), 0);
		unscored_tests += not keyval.second.score.has_value();
	});

	// Distribute score among tests with no score set
	if (unscored_tests > 0) {
		// max_score now tells how much score we have to distribute
		max_score = (max_score > 100 ? 0 : 100 - max_score);

		// Group 0 always has score equal to 0
		TestsGroup& first_group = tests_groups.front()->second;
		StringView first_gid = first_group.name;
		first_gid.removeLeading('0');
		if (first_gid.empty() and not first_group.score.has_value()) {
			first_group.score = 0;
			--unscored_tests;
			report_.append("Auto-scoring: score of the group with id `",
			               first_group.name, "` set to 0");
		}

		// Distribute scoring
		tests_groups.for_each([&](auto& keyval) {
			auto& score = keyval.second.score;
			if (not score.has_value()) {
				max_score -= (score = max_score / unscored_tests--).value();
				report_.append("Auto-scoring: score of the group with id `",
				               keyval.second.name, "` set to ", score.value());
			}
		});
	}

	bool run_model_solution = opts.reset_time_limits_using_model_solution;

	// Export tests to the Simfile
	sf.tgroups.clear();
	tests_groups.for_each([&](auto& keyval) {
		auto& group = keyval.second;
		// Sort tests in group
		sort(group.tests, [](const Test& a, const Test& b) {
			return Simfile::TestNameComparator()(a.name, b.name);
		});

		Simfile::TestGroup tg;
		tg.score = group.score.value();
		for (auto const& test : group.tests) {
			run_model_solution |= not test.time_limit.has_value();

			Simfile::Test t(
			   test.name.to_string(),
			   test.time_limit.value_or(std::chrono::nanoseconds(0)),
			   test.memory_limit.value());
			t.in = test.in.value().to_string();
			if (sf.interactive)
				t.out = std::nullopt;
			else
				t.out = test.out.value().to_string();

			tg.tests.emplace_back(std::move(t));
		}

		sf.tgroups.emplace_back(std::move(tg));
	});

	// Override the time limits
	if (opts.global_time_limit.has_value()) {
		run_model_solution = opts.reset_time_limits_using_model_solution;
		for (auto&& g : sf.tgroups)
			for (auto&& t : g.tests)
				t.time_limit = opts.global_time_limit.value();
	}

	if (not run_model_solution) {
		normalize_time_limits(sf);
		// Nothing more to do
		return {Status::COMPLETE, std::move(sf), master_dir.to_string()};

	} else { // The model solution's judge report is needed
		// Set the time limits for the model solution
		auto sol_time_limit =
		   std::chrono::duration_cast<std::chrono::milliseconds>(
		      time_limit_to_solution_runtime(
		         opts.max_time_limit,
		         opts.rtl_opts.solution_runtime_coefficient,
		         opts.rtl_opts.min_time_limit));
		for (auto&& g : sf.tgroups)
			for (auto&& t : g.tests)
				t.time_limit = sol_time_limit;

		return {Status::NEED_MODEL_SOLUTION_JUDGE_REPORT, std::move(sf),
		        master_dir.to_string()};
	}
}

void Conver::reset_time_limits_using_jugde_reports(
   Simfile& sf, const JudgeReport& jrep1, const JudgeReport& jrep2,
   const ResetTimeLimitsOptions& opts) {
	STACK_UNWINDING_MARK;

	using namespace std::chrono;
	using std::chrono_literals::operator""ns;

	if (opts.min_time_limit < 0ns)
		THROW("min_time_limit has to be non-negative");
	if (opts.solution_runtime_coefficient < 0)
		THROW("solution_runtime_coefficient has to be non-negative");

	// Map every test to its time limit
	AVLDictMap<StringView, nanoseconds> tls; // test name => time limit
	for (auto ptr : {&jrep1, &jrep2}) {
		auto&& rep = *ptr;
		for (auto&& g : rep.groups)
			for (auto&& t : g.tests) {
				// Only allow OK and WA to pass through
				if (not isOneOf(t.status, JudgeReport::Test::OK,
				                JudgeReport::Test::WA)) {
					THROW("Error on test `", t.name,
					      "`: ", JudgeReport::Test::description(t.status));
				}

				tls[t.name] = floor_to_10ms(solution_runtime_to_time_limit(
				   t.runtime, opts.solution_runtime_coefficient,
				   opts.min_time_limit));
			}
	}

	// Assign time limits to the tests
	for (auto&& tg : sf.tgroups)
		for (auto&& t : tg.tests)
			t.time_limit = tls[t.name];

	normalize_time_limits(sf);
}

} // namespace sim
