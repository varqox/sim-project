#include "../../include/libzip.h"
#include "../../include/parsers.h"
#include "../../include/sim/checker.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/sim/problem_package.h"
#include "default_checker_dump.h"

using std::string;
using std::vector;

namespace sim {

inline static vector<string> compile_command(SolutionLanguage lang,
	StringView source, StringView exec)
{
	// Makes vector of strings from arguments
	auto make = [](auto... args) {
		vector<string> res;
		res.reserve(sizeof...(args));
		(void)std::initializer_list<int>{
			(res.emplace_back(data(args), string_length(args)), 0)...
		};
		return res;
	};

	switch (lang) {
	case SolutionLanguage::C:
		return make("gcc", "-O2", "-std=c11", "-static", "-lm", "-m32", "-o",
			exec, "-xc", source);
	case SolutionLanguage::CPP:
		return make("g++", "-O2", "-std=c++11", "-static", "-lm", "-m32", "-o",
			exec, "-xc++", source);
	case SolutionLanguage::PASCAL:
		return make("fpc", "-O2", "-XS", "-Xt", concat("-o", exec), source);
	case SolutionLanguage::UNKNOWN:
		THROW("Invalid Language!");
	}

	THROW("Should not reach here");
}

constexpr meta::string JudgeWorker::CHECKER_FILENAME;
constexpr meta::string JudgeWorker::SOLUTION_FILENAME;
constexpr timespec JudgeWorker::CHECKER_TIME_LIMIT;

int JudgeWorker::compile_impl(FilePath source, SolutionLanguage lang,
	timespec time_limit, string* c_errors, size_t c_errors_max_len,
	const string& proot_path, StringView compilation_source_basename,
	StringView exec_dest_filename)
{
	auto compilation_dir = concat<PATH_MAX>(tmp_dir.path(), "compilation/");
	if (remove_r(compilation_dir) and errno != ENOENT)
		THROW("remove_r()", errmsg());

	if (mkdir(compilation_dir))
		THROW("mkdir()", errmsg());

	auto src_filename = concat<PATH_MAX>(compilation_source_basename);
	switch (lang) {
	case SolutionLanguage::C: src_filename.append(".c"); break;
	case SolutionLanguage::CPP: src_filename.append(".cpp"); break;
	case SolutionLanguage::PASCAL: src_filename.append(".pas"); break;
	case SolutionLanguage::UNKNOWN: THROW("Invalid language!");
	}

	if (copy(source, concat<PATH_MAX>(compilation_dir, src_filename)))
		THROW("copy()", errmsg());

	int rc = compile(compilation_dir,
		compile_command(lang, src_filename, exec_dest_filename), time_limit,
		c_errors, c_errors_max_len, proot_path);

	if (rc == 0 and move(
		concat<PATH_MAX>(compilation_dir, exec_dest_filename),
		concat<PATH_MAX>(tmp_dir.path(), exec_dest_filename)))
	{
		THROW("move()", errmsg());
	}

	return rc;
}

void JudgeWorker::loadPackage(string package_path, string simfile) {
	sf = Simfile {std::move(simfile)};
	sf.loadTestsWithFiles();
	sf.loadChecker();

	if (not sf.checker.has_value()) {
		// No checker is set, so place the default checker
		putFileContents(concat(tmp_dir.path(), "default_checker.c"),
			(const char*)default_checker_c, default_checker_c_len);
	}

	pkg_root = std::move(package_path);
	// Directory
	if (isDirectory(pkg_root)) {
		if (pkg_root.back() != '/')
			pkg_root += '/';

	// Zip
	} else {

		auto pkg_master_dir = zip_package_master_dir(pkg_root);
		// Collect all needed files in pc
		PackageContents pc;
		if (sf.checker.has_value())
			pc.add_entry(pkg_master_dir, sf.checker.value());

		for (auto&& tgroup : sf.tgroups)
			for (auto&& test : tgroup.tests) {
				pc.add_entry(pkg_master_dir, test.in);
				pc.add_entry(pkg_master_dir, test.out);
			}

		// Extract only the necessary files
		// TODO: find a way to extract files in the fly - e.g. during judging
		ZipFile zip(pkg_root, ZIP_RDONLY);
		auto eno = zip.entries_no();
		for (decltype(eno) i = 0; i < eno; ++i) {
			StringView ename = zip.get_name(i);
			if (pc.exists(ename)) {
				auto fpath = concat(tmp_dir.path(), "package/", ename);
				create_subdirectories(fpath);
				zip.extract_to_file(i, fpath);
			}
		}

		// Update root, so it will be relative to the Simfile
		pkg_root = concat_tostr(tmp_dir.path() + "package/", pkg_master_dir);
	}
}

Sandbox::ExitStat JudgeWorker::run_solution(FilePath input_file,
	FilePath output_file, uint64_t time_limit, uint64_t memory_limit) const
{
	// Solution STDOUT
	FileDescriptor solution_stdout(output_file, O_WRONLY | O_CREAT | O_TRUNC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", output_file, '`', errmsg());

	Sandbox sandbox;
	string solution_path {concat_tostr(tmp_dir.path(), SOLUTION_FILENAME)};

	FileDescriptor test_in(input_file, O_RDONLY);
	if (test_in < 0)
		THROW("Failed to open file `", input_file, '`', errmsg());

	// Set time limit to 1.5 * time_limit + 1 s, because CPU time is
	// measured
	uint64_t real_tl = time_limit * 3 / 2 + 1000000;
	timespec tl;
	tl.tv_sec = real_tl / 1000000;
	tl.tv_nsec = (real_tl - tl.tv_sec * 1000000) * 1000;

	timespec cpu_tl;
	cpu_tl.tv_sec = time_limit / 1000000;
	cpu_tl.tv_nsec = (time_limit - cpu_tl.tv_sec * 1000000) * 1000;

	// Run solution on the test
	Sandbox::ExitStat es = sandbox.run(solution_path, {}, {
		test_in,
		solution_stdout,
		-1,
		tl,
		memory_limit,
		cpu_tl
	}); // Allow exceptions to fly upper

	return es;
}

JudgeReport JudgeWorker::judge(bool final, JudgeLogger& judge_log) const {
	JudgeReport report;
	judge_log.begin(pkg_root, final);

	string sol_stdout_path {tmp_dir.path() + "sol_stdout"};
	// Checker STDOUT
	FileDescriptor checker_stdout {openUnlinkedTmpFile()};
	if (checker_stdout < 0)
		THROW("Failed to create unlinked temporary file", errmsg());

	// Solution STDOUT
	FileDescriptor solution_stdout(sol_stdout_path,
		O_WRONLY | O_CREAT | O_TRUNC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", sol_stdout_path, '`', errmsg());

	FileRemover solution_stdout_remover {sol_stdout_path}; // Save disk space

	// Checker parameters
	Sandbox::Options checker_opts = {
		-1, // STDIN is ignored
		checker_stdout, // STDOUT
		-1, // STDERR is ignored
		CHECKER_TIME_LIMIT,
		CHECKER_MEMORY_LIMIT
	};

	Sandbox sandbox;

	string checker_path {concat_tostr(tmp_dir.path(), CHECKER_FILENAME)};
	string solution_path {concat_tostr(tmp_dir.path(), SOLUTION_FILENAME)};

	for (auto&& group : sf.tgroups) {
		// Group "0" goes to the initial report, others groups to final
		auto p = Simfile::TestNameComparator::split(group.tests[0].name);
		if ((p.gid != "0") != final)
			continue;

		report.groups.emplace_back();
		auto& report_group = report.groups.back();

		double score_ratio = 1.0;

		for (auto&& test : group.tests) {
			// Prepare solution fds
			(void)ftruncate(solution_stdout, 0);
			(void)lseek(solution_stdout, 0, SEEK_SET);

			string test_in_path {pkg_root + test.in};
			string test_out_path {pkg_root + test.out};

			FileDescriptor test_in(test_in_path, O_RDONLY);
			if (test_in < 0)
				THROW("Failed to open file `", test_in_path, '`', errmsg());

			// Set time limit to 1.5 * time_limit + 1 s, because CPU time is
			// measured
			uint64_t real_tl = test.time_limit * 3 / 2 + 1000000;
			timespec tl;
			tl.tv_sec = real_tl / 1000000;
			tl.tv_nsec = (real_tl - tl.tv_sec * 1000000) * 1000;

			timespec cpu_tl;
			cpu_tl.tv_sec = test.time_limit / 1000000;
			cpu_tl.tv_nsec = (test.time_limit - cpu_tl.tv_sec * 1000000) * 1000;

			// Run solution on the test
			Sandbox::ExitStat es = sandbox.run(solution_path, {}, {
				test_in,
				solution_stdout,
				-1,
				tl,
				test.memory_limit,
				cpu_tl
			}); // Allow exceptions to fly upper

			// Update score_ratio
			// [0; 2/3 of time limit] => ratio = 1.0
			// (2/3 of time limit; time limit] => ratio linearly decreases to 0
			score_ratio = std::min(score_ratio, 3.0 -
				3.0 * (es.cpu_runtime.tv_sec * 100 +
					es.cpu_runtime.tv_nsec / 10000000)
				/ (test.time_limit / 10000));
			// cpu_runtime may be grater than test.time_limit therefore
			// score_ratio may be negative which is impermissible
			if (score_ratio < 0)
				score_ratio = 0;

			report_group.tests.emplace_back(
				test.name,
				JudgeReport::Test::OK,
				es.cpu_runtime.tv_sec * 1000000 + es.cpu_runtime.tv_nsec / 1000,
				test.time_limit,
				es.vm_peak,
				test.memory_limit,
				string {}
			);
			auto& test_report = report_group.tests.back();

			// OK
			if (es.si.code == CLD_EXITED and es.si.status == 0 and
				test_report.runtime <= test_report.time_limit)
			{

			// TLE
			} else if (test_report.runtime >= test_report.time_limit or
				es.runtime == tl)
			{
				// es.runtime == tl means that real_time_limit has been exceeded
				if (test_report.runtime < test_report.time_limit)
					test_report.runtime = test_report.time_limit;

				// After checking status for OK `>=` comparison is safe to
				// detect exceeding
				score_ratio = 0;
				test_report.status = JudgeReport::Test::TLE;
				test_report.comment = "Time limit exceeded";
				judge_log.test(test.name, test_report, es);
				continue;

			// MLE
			} else if (es.message == "Memory limit exceeded" ||
				test_report.memory_consumed > test_report.memory_limit)
			{
				score_ratio = 0;
				test_report.status = JudgeReport::Test::MLE;
				test_report.comment = "Memory limit exceeded";
				judge_log.test(test.name, test_report, es);
				continue;

			// RTE
			} else {
				score_ratio = 0;
				test_report.status = JudgeReport::Test::RTE;
				test_report.comment = "Runtime error";
				if (es.message.size())
					back_insert(test_report.comment, " (", es.message, ')');

				judge_log.test(test.name, test_report, es);
				continue;
			}

			// Status == OK

			/* Checking solution output with checker */

			// Prepare checker fds
			(void)ftruncate(checker_stdout, 0);
			(void)lseek(checker_stdout, 0, SEEK_SET);

			// Run checker
			auto ces = sandbox.run(checker_path,
				{checker_path, test_in_path, test_out_path, sol_stdout_path},
				checker_opts,
				{
					{test_in_path, OpenAccess::RDONLY},
					{test_out_path, OpenAccess::RDONLY},
					{sol_stdout_path, OpenAccess::RDONLY}
				}); // Allow exceptions to fly higher

			InplaceBuff<4096> checker_error_str;
			// Checker exited with 0
			if (ces.si.code == CLD_EXITED and ces.si.status == 0) {
				string chout = sim::obtainCheckerOutput(checker_stdout, 512);
				SimpleParser s {chout};

				StringView line1 {s.extractNext('\n')}; // "OK" or "WRONG"
				StringView line2 {s.extractNext('\n')}; // percentage (real)

				auto wrong_second_line = [&] {
					score_ratio = 0; // Do not give score for a checker error
					test_report.status = JudgeReport::Test::CHECKER_ERROR;
					test_report.comment = "Checker error";
					checker_error_str.append("Second line of the stdout is"
						" invalid: `", line2, "` - it has be to either empty or"
						" a real number representing the percentage of score"
						" that solution will receive");
				};

				// Second line has to be either empty or be a real number
				if (line2.size() && (line2[0] == '-' || !isReal(line2))) {
					wrong_second_line();

				// "OK" -> Checker: OK
				} else if (line1 == "OK") {
					// Test status is already set to OK
					// line2 format was checked above
					if (line2.size()) { // Empty line means 100%
						errno = 0;
						char* ptr;
						double x = strtod(line2.data(), &ptr);
						if (errno != 0 or ptr == line2.data())
							wrong_second_line();
						else
							score_ratio = std::min(score_ratio, x * 0.01);
					}

					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);

				// "WRONG" -> Checker: WA
				} else if (line1 == "WRONG") {
					test_report.status = JudgeReport::Test::WA;
					score_ratio = 0; // Ignoring second line
					// Leave the checker comment only
					chout.erase(chout.begin(), chout.end() - s.size());
					test_report.comment = std::move(chout);

				// * -> Checker error
				} else {
					score_ratio = 0; // Do not give score for a checker error
					test_report.status = JudgeReport::Test::CHECKER_ERROR;
					test_report.comment = "Checker error";
					checker_error_str.append("First line of the stdout is"
						" invalid: `", line1, "` - it has to be either `OK` or"
						" `WRONG`");
				}

			// Checker TLE
			} else if (ces.runtime >= CHECKER_TIME_LIMIT) {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker error";
				checker_error_str.append("Time limit exceeded");

			// Checker MLE
			} else if (ces.message == "Memory limit exceeded" ||
				ces.vm_peak > CHECKER_MEMORY_LIMIT)
			{
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker error";
				checker_error_str.append("Memory limit exceeded");

			// Checker RTE
			} else {
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker error";
				checker_error_str.append("Runtime error");
				if (ces.message.size())
					checker_error_str.append(" (", ces.message, ')');
			}

			// Logging
			judge_log.test(test.name, test_report, es, ces, CHECKER_MEMORY_LIMIT,
				checker_error_str);
		}

		// Compute group score
		report_group.score = round(group.score * score_ratio);
		report_group.max_score = group.score;

		judge_log.group_score(report_group.score, report_group.max_score, score_ratio);
	}

	judge_log.end();
	report.judge_log = judge_log.render_judge_log();
	return report;
}

} // namespace sim
