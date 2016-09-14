#include "../../include/debug.h"
#include "../../include/sim/conver.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/spawner.h"
#include "../../include/utilities.h"
#include "default_checker_dump.h"

using std::map;
using std::runtime_error;
using std::string;
using std::vector;

namespace sim {

void Conver::extractPackage(const string& package_path) {
	// Clear the temporary directory
	if (tmp_dir_.exist())
		removeDirContents(tmp_dir_.path());
	else
		tmp_dir_ = TemporaryDirectory("/tmp/sim-conver.XXXXXX");

	// Set package_path_
	package_path_ = tmp_dir_.path() + "p/";
	if (mkdir(package_path_))
		THROW("mkdir(`", package_path_, "`)", error(errno));

	/* Copy the package to the temporary directory */

	// Directory
	if (isDirectory(package_path)) {
		if (verbose_)
			stdlog("Copying package...");

		if (copy_r(package_path, package_path_))
			THROW("Failed to copy package: copy_r()", error(errno));

	// Archive
	} else {
		vector<string> args;

		// zip
		if (hasSuffix(package_path, ".zip"))
			back_insert(args, "unzip", (verbose_ ? "-o" : "-oq"), package_path,
				"-d", package_path_);
		// tar.gz | tgz
		else if (hasSuffixIn(package_path, {".tgz", ".tar.gz"}))
			back_insert(args, "tar", (verbose_ ? "xvzf" : "xzf"), package_path,
				"-C", package_path_);
		else
			THROW("Unsupported archive type");

		/* Unpack the package */

		if (verbose_)
			stdlog("Unpacking package...");

		Spawner::ExitStat es = Spawner::run(args[0], args,
			{-1, stdlog.fileno(), stdlog.fileno(), 0, 512 << 20});

		if (es.code)
			THROW("Unpacking error: ", es.message);

		if (verbose_)
			stdlog("Unpacking took ", usecToSecStr(es.runtime, 3), "s.");
	}

	// Check if the package has the master directory
	int entities = 0;
	string master_dir;
	forEachDirComponent(package_path_, [&](dirent* file) {
		++entities;
		if (master_dir.empty() && file->d_type == DT_DIR)
			master_dir = file->d_name;
	});

	// If package has the master directory, update package_path_
	if (master_dir.size() && entities == 1)
		back_insert(package_path_, master_dir, '/');
}

Simfile Conver::constructFullSimfile(const Options& opts) {
	auto dt = directory_tree::dumpDirectoryTree(package_path_);
	dt->removeDir("utils"); // This directory is ignored by Conver

	// Load the Simfile from the package
	Simfile sf;
	bool simfile_is_loaded = false;
	if (!opts.ignore_simfile && dt->fileExists("Simfile")) {
		simfile_is_loaded = true;
		sf = Simfile {getFileContents(package_path_ + "Simfile")};
	}

	// Name
	if (opts.name.size())
		sf.name = opts.name;
	else if ((sf.name = sf.configFile()["name"].asString()).empty())
		throw runtime_error("Problem name is not specified");

	// Tag
	if (opts.tag.size())
		sf.tag = opts.tag;
	else if ((sf.tag = sf.configFile()["tag"].asString()).empty())
		sf.tag = makeTag(sf.name);

	auto is_source = [](const string& file) {
		return hasSuffixIn(file, {".c", ".cc", ".cpp", ".cxx"});
	};

	// Checker
	try { sf.loadChecker(); } catch (...) {}
	if (!dt->pathExists(sf.checker)) {
		if (verbose_ && simfile_is_loaded)
			stdlog("Missing / invalid checker specified in the package Simfile"
				" - ignoring");

		// Scan check/ directory
		auto x = findFiles(dt->dir("check"), is_source, "check/");
		if (x.size()) {
			sf.checker = x.front();
		} else {
			(void)mkdir(concat(package_path_, "check"));
			sf.checker = "check/checker.c";
			putFileContents((package_path_ + sf.checker).data(),
				(const char*)default_checker_c, default_checker_c_len);
		}
	}

	dt->removeDir("check"); // Exclude check/ directory from future searches

	// Statement
	try { sf.loadStatement(); } catch (...) {}
	if (!dt->pathExists(sf.statement)) {
		if (verbose_ && simfile_is_loaded)
			stdlog("Missing / invalid statement specified in the package "
				"Simfile - ignoring");

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
	vector<std::string> solutions;
	for (auto&& s : sf.solutions) {
		if (dt->pathExists(s))
			solutions.emplace_back(s);
		else if (verbose_)
			stdlog("Ignoring invalid solution: `", s, '`');
	}
	{
		auto x = findFiles(dt.get(), is_source);
		if (solutions.empty()) { // The main solution has to be set
			if (x.empty())
				throw runtime_error("No solution was found");

			// Choose the one with the shortest path
			swap(x.front(), *std::min_element(x.begin(), x.end(),
				[](const string& a, const string& b) {
					return a.size() < b.size();
				}));
		}

		// Merge solutions and put them in the Simfile
		solutions.insert(solutions.end(), x.begin(), x.end());
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
		for (auto&& s : outs) {
			StringView name {s};
			name.removeSuffix(4);
			name = name.extractTrailing([](char c) { return (c != '/'); });

			auto it = in.find(name.to_string()); // TODO: if C++14, remove part:
			                                     // `.to_string()`
			if (it == in.end()) // No matching .in file
				continue;

			tests.emplace_back(std::move(it->first), 0, 0);
			tests.back().in = std::move(it->second);
			tests.back().out = std::move(s);
			in.erase(it); // Now *it is not valid, so it's better to remove it
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
			if (git.first == "0")
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
					"Simfile nor options");

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
		sf.global_mem_limit = opts.memory_limit << 10; // Convert from KB to
		                                               // bytes
			for (auto&& g : sf.tgroups)
				for (auto&& t : g.tests)
					t.memory_limit = sf.global_mem_limit;
	}

	if (!run_time_limits_setting)
		return sf; // Nothing more to do

	/* Set time limits automatically */

	constexpr uint64_t TIME_LIMIT_ADJUSTMENT = 0.4e6; // 0.4 s
	constexpr int MODEL_TIME_COEFFICENT = 4;

	uint64_t time_limit =
		(std::max<uint64_t>(opts.max_time_limit, TIME_LIMIT_ADJUSTMENT) -
			TIME_LIMIT_ADJUSTMENT) / MODEL_TIME_COEFFICENT;

	for (auto&& g : sf.tgroups)
		for (auto&& t : g.tests)
			t.time_limit = time_limit;

	JudgeWorker jworker;
	jworker.setVerbosity(verbose_);

	try { jworker.loadPackage(package_path_, sf.dump()); } catch(...) {
		if (verbose_)
			stdlog("Generated Simfile: ", sf.dump());
		throw;
	}

	string compilation_errors;

	// Compile checker
	if (verbose_)
		stdlog("Compiling checker...");
	if (jworker.compileChecker(opts.compilation_time_limit, &compilation_errors,
		opts.compilation_errors_max_length, opts.proot_path))
	{
		if (verbose_)
			stdlog("Checker compilation failed.");
		throw runtime_error(concat("Checker compilation failed:\n",
			compilation_errors));
	}

	// Compile solution
	if (verbose_)
		stdlog("Compiling the main solution...");
	if (jworker.compileSolution(package_path_ + sf.solutions.front(),
		opts.compilation_time_limit, &compilation_errors,
		opts.compilation_errors_max_length, opts.proot_path))
	{
		if (verbose_)
			stdlog("The main solution compilation failed.");
		throw runtime_error(concat("The main solution compilation failed:\n",
			compilation_errors));
	}

	// Judge (obtain time limits)
	JudgeReport rep1 = jworker.judge(false);
	JudgeReport rep2 = jworker.judge(true);

	// Map every test to its time limit
	map<string, uint64_t> tls; // test name => time limit
	for (auto&& rep : {rep1, rep2})
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

	// Assign time limits to tests
	for (auto&& tg : sf.tgroups)
		for (auto&& t : tg.tests)
			t.time_limit = tls[t.name];

	return sf;
}

} // namespace sim
