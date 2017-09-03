#include "../../include/debug.h"
#include "../../include/sim/conver.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/sim/problem_package.h"
#include "../../include/spawner.h"
#include "../../include/utilities.h"
#include "../../include/zip.h"
#include "default_checker_dump.h"

using std::string;
using std::vector;

namespace sim {

Conver::ConstructionResult Conver::constructSimfile(const Options& opts) {
	/* Load contents of the package */

	PackageContents pc;
	// Directory
	if (isDirectory(package_path_)) {
		throw_assert(package_path_.size() > 0);
		if (package_path_.back() != '/')
			package_path_ += '/';
		// From now on, package_path_ has trailing '/' iff it is a directory

		pc.load_from_directory(package_path_);

	// Zip archive
	} else
		pc.load_from_zip(package_path_);

	// Find master directory if exists
	StringView master_dir = pc.master_dir();

	auto exists_in_pkg = [&](StringView file) {
		return (not file.empty() and pc.exists(concat(master_dir, file)));
	};

	// "utils/" directory is ignored by Conver
	pc.remove_with_prefix(concat(master_dir, "utils/"));

	// Load the Simfile from the package
	Simfile sf;
	bool simfile_is_loaded = false;
	if (!opts.ignore_simfile && exists_in_pkg("Simfile")) {
		simfile_is_loaded = true;
		try {
			sf = Simfile(package_path_.back() == '/' ?
				getFileContents(concat(package_path_, master_dir, "Simfile")
					.to_cstr())
				: extract_file_from_zip(package_path_,
					concat(master_dir, "Simfile").to_cstr()));
		} catch (const ConfigFile::ParseError& e) {
			THROW(concat_tostr("(Simfile) ", e.what(), '\n', e.diagnostics()));
		}
	}

	// Name
	if (opts.name.size()) {
		sf.name = opts.name;
		if (verbose_)
			report_.append("Problem's name (specified in options): ", sf.name);
	} else {
		if (verbose_)
			report_.append("Problem's name is not specified in options -"
				" loading it from Simfile");
		sf.loadName();
		if (verbose_)
			report_.append("  name loaded from Simfile: ", sf.name);
	}

	// Label
	if (opts.label.size()) {
		sf.label = opts.label;
		if (verbose_)
			report_.append("Problem's label (specified in options): ",
				sf.label);
	} else {
		if (verbose_)
			report_.append("Problem's label is not specified in options -"
				" loading it from Simfile");
		try {
			sf.loadLabel();
			if (verbose_)
				report_.append("  label loaded from Simfile: ", sf.label);

		} catch (const std::exception& e) {
			if (verbose_)
				report_.append("  ", e.what(), " -> generating label from the"
					" problem's name");

			sf.label = shortenName(sf.name);
			if (verbose_)
				report_.append("  label generated from name: ", sf.label);
		}
	}

	auto is_source = [](StringView file) {
		return hasSuffixIn(file, {".c", ".cc", ".cpp", ".cxx"});
	};

	auto collect_files = [&pc](StringView prefix, auto&& cond) {
		vector<StringView> res;
		pc.for_each_with_prefix(prefix, [&](StringView file) {
			if (cond(file))
				res.emplace_back(file);
		});
		return res;
	};

	// Checker
	try { sf.loadChecker(); } catch (...) {}
	if (not exists_in_pkg(sf.checker)) {
		if (verbose_ && simfile_is_loaded)
			report_.append("Missing / invalid checker specified in the "
				"package's Simfile - searching for one");

		// Scan check/ directory
		auto x = collect_files(concat(master_dir, "check/"), is_source);
		if (x.size()) {
			sf.checker = x.front().substr(master_dir.size()).to_string();
			if (verbose_)
				report_.append("Chosen checker: ", sf.checker);
		// We have to put the checker in the package
		} else {
			if (verbose_)
				report_.append("No checker was found - placing the default"
					" checker");

			sf.checker = concat_tostr("check/checker.c");
			if (package_path_.back() == '/') {
				(void)mkdir(concat(package_path_, "check").to_cstr());
				putFileContents(
					concat(package_path_, master_dir, sf.checker).to_cstr(),
					(const char*)default_checker_c, default_checker_c_len);
			} else
				update_add_data_to_zip(
					{(const char*)default_checker_c, default_checker_c_len},
					concat(master_dir, sf.checker).to_cstr(), package_path_);
		}
	}

	// Exclude check/ directory from future searches
	pc.remove_with_prefix(concat(master_dir, "check/"));

	// Statement
	try { sf.loadStatement(); } catch (...) {}
	if (not exists_in_pkg(sf.statement)) {
		if (verbose_ && simfile_is_loaded)
			report_.append("Missing / invalid statement specified in the "
				"package's Simfile - searching for one");

		auto is_statement = [](StringView file) {
			return hasSuffixIn(file, {".pdf", ".html", ".htm", ".md", ".txt"});
		};

		// Scan doc/ directory
		auto x = collect_files(concat(master_dir, "doc/"), is_statement);
		if (x.empty())
			x = collect_files("", is_statement); // Scan whole directory tree

		// If there is no statement
		if (x.empty())
			THROW("No statement was found");

		// Prefer PDF to other formats
		auto it = find_if(x.begin(), x.end(),
			[](StringView s) { return hasSuffix(s, ".pdf"); });
		sf.statement = (it != x.end() ? *it : x.front())
			.substr(master_dir.size()).to_string();

		if (verbose_)
			report_.append("Chosen statement: ", sf.statement);
	}

	// Solutions
	try { sf.loadSolutions(); } catch (...) {}
	{
		vector<std::string> solutions;
		AVLDictSet<std::string> solutions_set; // Used to detect and eliminate
		                                // solutions duplication
		for (auto&& sol : sf.solutions) {
			if (exists_in_pkg(sol) && not solutions_set.find(sol)) {
				solutions.emplace_back(sol);
				solutions_set.emplace(sol);
			} else if (verbose_)
				report_.append("Ignoring invalid solution: `", sol, '`');
		}

		auto x = collect_files("", is_source);
		if (solutions.empty()) { // The main solution has to be set
			if (x.empty())
				THROW("No solution was found");

			// Choose the one with the shortest path to be the model solution
			std::swap(x.front(), *std::min_element(x.begin(), x.end(),
				[](auto&& a, auto&& b) {
					return a.size() < b.size();
				}));
		}

		// Merge solutions
		for (auto&& s : x) {
			s.removePrefix(master_dir.size());
			if (solutions_set.emplace(s.to_string()))
				solutions.emplace_back(s.to_string());
		}

		// Put the solutions into the Simfile
		sf.solutions = std::move(solutions);
	}

	// TODO: allow specifying scoring without limits
	//

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

		auto ins = collect_files("",
			[](StringView file) { return hasSuffix(file, ".in"); });
		auto outs = collect_files("",
			[](StringView file) { return hasSuffix(file, ".out"); });

		AVLDictMap<string, string> in; // test name => path
		for (auto&& s : ins) {
			StringView name {s};
			name.removeSuffix(3);
			name = name.extractTrailing([](char c) { return (c != '/'); });
			in.emplace(name.to_string(),
				s.substr(master_dir.size()).to_string());
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
				if (not it) // No matching .in file
					continue;

				tests.emplace_back(std::move(it->first), 0, 0);
				tests.back().in = std::move(it->second);
				tests.back().out = s.substr(master_dir.size()).to_string();
				// Now *it is not valid, so it's better to remove it
				in.erase(it->first);
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
						THROW("Input and output files not found for the test `",
							test.name, '`');

					test.in = std::move(it->in);
					test.out = std::move(it->out);
				}

			break;
		}

		// There are no tests limits specified
		AVLDictMap<string, Simfile::TestGroup, StrNumCompare> tg; // gid => test
		for (auto&& t : tests) {
			auto p = Simfile::TestNameComparator::split(t.name);
			// tid == "ocen" belongs to the same group as tests with gid == "0"
			string gid = (p.second == "ocen" ? "0" : p.first.to_string()); //
			    // Needed (.to_string()) because since t is moved, p.first may
			    // not be valid
			tg[std::move(gid)].tests.emplace_back(std::move(t));
		}

		// Move tests groups from tg to sf.tgroups
		int groups_no = tg.size() - bool(tg.find("0"));
		int total_score = 100;
		tg.for_each([&](auto&& git) {
			// Take care of the scoring
			StringView x = git.first;
			x.removeLeading('0');
			if (git.first.size() && x.empty()) // We have to compare as numbers
				git.second.score = 0;
			else
				total_score -= (git.second.score = total_score / groups_no--);

			sf.tgroups.emplace_back(std::move(git.second));
		});

		// Take care of the time limit
		if (opts.global_time_limit == 0)
			run_time_limits_setting = true;

		// Take care of the memory limit
		if (opts.memory_limit == 0) {
			if (sf.global_mem_limit == 0)
				THROW("No memory limit is specified in both Simfile and"
					" options");

			for (auto&& g : sf.tgroups)
				for (auto&& t : g.tests)
					t.memory_limit = sf.global_mem_limit;
		}
	} while(false);

	// Overwrite the time limit
	if (opts.global_time_limit > 0)
		for (auto&& g : sf.tgroups)
			for (auto&& t : g.tests)
				t.time_limit = opts.global_time_limit;

	// Overwrite the memory limit
	if (opts.memory_limit > 0) {
		sf.global_mem_limit = opts.memory_limit << 20; // Convert from MB to
		                                               // bytes
			for (auto&& g : sf.tgroups)
				for (auto&& t : g.tests)
					t.memory_limit = sf.global_mem_limit;
	}

	if (!run_time_limits_setting) {
		normalize_time_limits(sf);
		// Nothing more to do
		return {Status::COMPLETE, std::move(sf), master_dir.to_string()};
	}

	/* The model solution's judge report is needed */

	// Set the time limits for the model solution
	for (auto&& g : sf.tgroups)
		for (auto&& t : g.tests)
			t.time_limit = MODEL_SOL_TLIMIT;

	return {Status::NEED_MODEL_SOLUTION_JUDGE_REPORT, std::move(sf),
		master_dir.to_string()};
}

void Conver::finishConstructingSimfile(Simfile &sf, const JudgeReport &jrep1,
	const JudgeReport &jrep2)
{
	// Map every test to its time limit
	AVLDictMap<string, uint64_t> tls; // test name => time limit
	for (auto ptr : {&jrep1, &jrep2}) {
		auto&& rep = *ptr;
		for (auto&& g : rep.groups)
			for (auto&& t : g.tests){
				// Only allow OK and WA to pass through
				if (!isIn(t.status,
					{JudgeReport::Test::OK, JudgeReport::Test::WA}))
				{
					THROW("Error on test `", t.name, "`: ", JudgeReport::Test::description(t.status));
				}

				tls[std::move(t.name)] = time_limit(t.runtime / 1e6) * 1e6;
			}
	}

	// Assign time limits to the tests
	for (auto&& tg : sf.tgroups)
		for (auto&& t : tg.tests)
			t.time_limit = tls[t.name];

	normalize_time_limits(sf);
}

} // namespace sim
