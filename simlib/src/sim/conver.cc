#include "../../include/debug.h"
#include "../../include/sim/conver.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/spawner.h"
#include "../../include/utilities.h"
#include "default_checker_dump.h"

#include <set>

using std::map;
using std::pair;
using std::runtime_error;
using std::set;
using std::string;
using std::vector;

namespace sim {

pair<Conver::Status, Simfile> Conver::constructSimfile(const Options& opts) {
	auto dt = directory_tree::dumpDirectoryTree(package_path_);
	dt->removeDir("utils"); // This directory is ignored by Conver

	// Load the Simfile from the package
	Simfile sf;
	bool simfile_is_loaded = false;
	if (!opts.ignore_simfile && dt->fileExists("Simfile")) {
		simfile_is_loaded = true;
		try {
			sf = Simfile {getFileContents(package_path_ + "Simfile")};
		} catch (const ConfigFile::ParseError& e) {
			throw ConfigFile::ParseError{concat("(Simfile) ", e.what())};
		}
	}

	// Name
	if (opts.name.size())
		sf.name = opts.name;
	else {
		report_.append("Problem's name is not specified in options - loading it"
			" from Simfile");
		sf.loadName();
	}

	// Label
	if (opts.label.size())
		sf.label = opts.label;
	else {
		report_.append("Problem's label is not specified in options - loading"
			" it from Simfile");
		try { sf.loadLabel(); } catch (const std::exception& e) {
			report_.append(e.what(),
				" -> generating label from the problem's name");
			sf.label = shortenName(sf.name);
		}
	}

	auto is_source = [](const string& file) {
		return hasSuffixIn(file, {".c", ".cc", ".cpp", ".cxx"});
	};

	// Checker
	try { sf.loadChecker(); } catch (...) {}
	if (!dt->pathExists(sf.checker)) {
		if (verbose_ && simfile_is_loaded)
			report_.append("Missing / invalid checker specified in the "
				"package's Simfile - ignoring");

		// Scan check/ directory
		auto x = findFiles(dt->dir("check"), is_source, "check/");
		if (x.size()) {
			sf.checker = x.front();
		} else {
			(void)mkdir(concat(package_path_, "check"));
			sf.checker = "check/checker.c";
			putFileContents(package_path_ + sf.checker,
				(const char*)default_checker_c, default_checker_c_len);
		}
	}

	dt->removeDir("check"); // Exclude check/ directory from future searches

	// Statement
	try { sf.loadStatement(); } catch (...) {}
	if (!dt->pathExists(sf.statement)) {
		if (verbose_ && simfile_is_loaded)
			report_.append("Missing / invalid statement specified in the "
				"package's Simfile - ignoring");

		auto is_statement = [](const string& file) {
			return hasSuffixIn(file, {".pdf", ".html", ".htm", ".md", ".txt"});
		};

		// Scan doc/ directory
		auto x = findFiles(dt->dir("doc"), is_statement, "doc/");
		if (x.empty())
			x = findFiles(dt.get(), is_statement); // Scan whole directory tree

		// If there is no statement
		if (x.empty())
			throw runtime_error("No statement was found");

		// Prefer PDF to other formats
		auto it = find_if(x.begin(), x.end(),
			[](const string& s) { return hasSuffix(s, ".pdf"); });
		sf.statement = (it != x.end() ? *it : x.front());
	}

	// Solutions
	try { sf.loadSolutions(); } catch (...) {}
	{
		vector<std::string> solutions;
		set<std::string> solutions_set; // Used to detect and eliminate
		                                // solutions duplication
		for (auto&& s : sf.solutions) {
			if (dt->pathExists(s) &&
				solutions_set.find(s) == solutions_set.end())
			{
				solutions.emplace_back(s);
				solutions_set.emplace(s);
			}
			else if (verbose_)
				report_.append("Ignoring invalid solution: `", s, '`');
		}

		auto x = findFiles(dt.get(), is_source);
		if (solutions.empty()) { // The main solution has to be set
			if (x.empty())
				throw runtime_error("No solution was found");

			// Choose the one with the shortest path to be the model solution
			swap(x.front(), *std::min_element(x.begin(), x.end(),
				[](auto&& a, auto&& b) {
					return a.size() < b.size();
				}));
		}

		// Merge solutions
		for (auto&& s : x)
			if (solutions_set.emplace(s).second)
				solutions.emplace_back(s);

		// Put the solutions into the Simfile
		sf.solutions = std::move(solutions);
	}

	// Tests
	try { sf.loadTests(); } catch (...) {
		sf.tgroups.clear();
	}

	bool run_time_limits_setting = opts.force_auto_time_limits_setting &&
		opts.global_time_limit == 0;
	do {
		// If limits were loaded successfully
		if (sf.tgroups.size())
			try {
				sf.loadTestsFiles();
				break;
			} catch (...) {}

		auto ins = findFiles(dt.get(),
			[](const string& file) { return hasSuffix(file, ".in"); });
		auto outs = findFiles(dt.get(),
			[](const string& file) { return hasSuffix(file, ".out"); });

		map<string, string> in; // test name => path
		for (auto&& s : ins) {
			StringView name {s};
			name.removeSuffix(3);
			name = name.extractTrailing([](char c) { return (c != '/'); });

			string x {name.to_string()}; // Needed because since s is moved,
			                             // name may not be valid
			in.emplace(std::move(x), std::move(s));
		}

		// Construct a vector of the valid tests
		vector<Simfile::Test> tests;
		{
			string name_s; // Placed here to save useless allocations
			for (auto&& s : outs) {
				StringView name {s};
				name.removeSuffix(4);
				name = name.extractTrailing([](char c) { return (c != '/'); });
				name_s.assign(name.begin(), name.end()); // Place in a string

				auto it = in.find(name_s);
				if (it == in.end()) // No matching .in file
					continue;

				tests.emplace_back(std::move(it->first), 0, 0);
				tests.back().in = std::move(it->second);
				tests.back().out = std::move(s);
				// Now *it is not valid, so it's better to remove it
				in.erase(it);
			}
		}

		// Sort to speed up searching
		auto test_cmp = [](const Simfile::Test& a, const Simfile::Test& b) {
			return a.name < b.name;
		};
		sort(tests, test_cmp);

		// If there are tests limits specified, but we have to load tests' files
		if (sf.tgroups.size()) {
			for (auto&& group : sf.tgroups)
				for (auto&& test : group.tests) {
					auto it = binaryFind(tests, test, test_cmp);
					if (it == tests.end())
						throw runtime_error(concat("Input and output files not "
							"found for the test `", test.name, '`'));

					test.in = std::move(it->in);
					test.out = std::move(it->out);
				}

			break;
		}

		// There are no tests limits specified
		map<string, Simfile::TestGroup, StrNumCompare> tg; // gid => test
		for (auto&& t : tests) {
			auto p = Simfile::TestNameComparator::split(t.name);
			// tid == "ocen" belongs to the same group as tests with gid == "0"
			string gid = (p.second == "ocen" ? "0" : p.first.to_string()); //
			    // Needed (.to_string()) because since t is moved, p.first may
			    // not be valid
			tg[std::move(gid)].tests.emplace_back(std::move(t));
		}

		// Move tests groups from tg to sf.tgroups
		int groups_no = tg.size() - (tg.find("0") != tg.end());
		int total_score = 100;
		for (auto&& git : tg) {
			// Take care of the scoring
			StringView x = git.first;
			x.removeLeading('0');
			if (git.first.size() && x.empty()) // We have to compare as numbers
				git.second.score = 0;
			else
				total_score -= (git.second.score = total_score / groups_no--);

			sf.tgroups.emplace_back(std::move(git.second));
		}

		// Take care of the time limit
		if (opts.global_time_limit == 0)
			run_time_limits_setting = true;

		// Take care of the memory limit
		if (opts.memory_limit == 0) {
			if (sf.global_mem_limit == 0)
				throw runtime_error("No memory limit is specified in neither "
					"Simfile nor in options");

			for (auto&& g : sf.tgroups)
				for (auto&& t : g.tests)
					t.memory_limit = sf.global_mem_limit;
		}
	} while(false);

	// Overwrite time limit
	if (opts.global_time_limit > 0)
		for (auto&& g : sf.tgroups)
			for (auto&& t : g.tests)
				t.time_limit = opts.global_time_limit;

	// Overwrite memory limit
	if (opts.memory_limit > 0) {
		sf.global_mem_limit = opts.memory_limit << 20; // Convert from MB to
		                                               // bytes
			for (auto&& g : sf.tgroups)
				for (auto&& t : g.tests)
					t.memory_limit = sf.global_mem_limit;
	}

	if (!run_time_limits_setting) {
		normalize_time_limits(sf);
		return {Status::COMPLETE, sf}; // Nothing more to do
	}

	/* Judge report of the model solution is needed */

	// Set the time limits for the model solution
	uint64_t time_limit =
		(meta::max(opts.max_time_limit, (uint64_t)TIME_LIMIT_ADJUSTMENT) -
			TIME_LIMIT_ADJUSTMENT) / MODEL_TIME_COEFFICENT;

	for (auto&& g : sf.tgroups)
		for (auto&& t : g.tests)
			t.time_limit = time_limit;

	return {Status::NEED_MODEL_SOLUTION_JUDGE_REPORT, sf};
}

void Conver::finishConstructingSimfile(Simfile &sf, const JudgeReport &jrep1,
	const JudgeReport &jrep2)
{
	// Map every test to its time limit
	map<string, uint64_t> tls; // test name => time limit
	for (auto ptr : {&jrep1, &jrep2}) {
		auto&& rep = *ptr;
		for (auto&& g : rep.groups)
			for (auto&& t : g.tests){
				// Only allow OK and WA to pass through
				if (!isIn(t.status,
					{JudgeReport::Test::OK, JudgeReport::Test::WA}))
				{
					throw runtime_error(concat("Error on test `", t.name, "`: ",
						JudgeReport::Test::description(t.status)));
				}

				tls[std::move(t.name)] = TIME_LIMIT_ADJUSTMENT + t.runtime *
					MODEL_TIME_COEFFICENT;
			}
	}

	// Assign time limits to the tests
	for (auto&& tg : sf.tgroups)
		for (auto&& t : tg.tests)
			t.time_limit = tls[t.name];

	normalize_time_limits(sf);
}

} // namespace sim
