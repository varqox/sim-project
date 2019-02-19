#include "../../include/libzip.h"
#include "../../include/parsers.h"
#include "../../include/sim/checker.h"
#include "../../include/sim/judge_worker.h"
#include "../../include/sim/problem_package.h"
#include "default_checker_dump.h"

using std::string;
using std::vector;

namespace sim {

class DirPackageLoader : public PackageLoader {
	InplaceBuff<PATH_MAX> pkg_root_;

public:
	DirPackageLoader(FilePath pkg_path) : pkg_root_(pkg_path) {
		if (pkg_root_.size and pkg_root_.back() != '/')
			pkg_root_.append('/');
	}

	std::string load_into_dest_file(FilePath path, FilePath dest) override {
		if (copy(concat(pkg_root_, path), dest))
			THROW("copy()", errmsg());

		return dest.to_str();
	}

	std::string load_as_file(FilePath path, FilePath) override {
		auto res = concat_tostr(pkg_root_, path);
		if (access(res, F_OK) != 0)
			THROW("load_as_file() - Such file does not exist");

		return res;
	}

	std::string load_as_str(FilePath path) override {
		return getFileContents(concat(pkg_root_, path));
	}

	virtual ~DirPackageLoader() = default;
};

class ZipPackageLoader : public PackageLoader {
	TemporaryDirectory& tmp_dir_;
	ZipFile zip_;
	std::string pkg_master_dir_;

	auto as_pkg_path(FilePath path) { return concat(pkg_master_dir_, path); }

public:
	ZipPackageLoader(TemporaryDirectory& tmp_dir, FilePath pkg_path)
		: tmp_dir_(tmp_dir), zip_(pkg_path, ZIP_RDONLY),
			pkg_master_dir_(sim::zip_package_master_dir(pkg_path)) {}

	std::string load_into_dest_file(FilePath path, FilePath dest) override {
		zip_.extract_to_file(zip_.get_index(as_pkg_path(path)), dest);
		return dest.to_str();
	}

	std::string load_as_file(FilePath path, FilePath hint_name) override {
		auto dest = concat_tostr(tmp_dir_.path(), "pkg_loader_", hint_name);
		zip_.extract_to_file(zip_.get_index(as_pkg_path(path)), dest);
		return dest;
	}

	std::string load_as_str(FilePath path) override {
		return zip_.extract_to_str(zip_.get_index(as_pkg_path(path)));
	}

	virtual ~ZipPackageLoader() = default;
};

inline static vector<string> compile_command(SolutionLanguage lang,
	StringView source, StringView exec)
{
	STACK_UNWINDING_MARK;

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
	case SolutionLanguage::C11:
		return make("gcc", "-O2", "-std=c11", "-static", "-lm", "-m32", "-o",
			exec, "-xc", source);
	case SolutionLanguage::CPP11:
		return make("g++", "-O2", "-std=c++11", "-static", "-lm", "-m32", "-o",
			exec, "-xc++", source);
	case SolutionLanguage::CPP14:
		return make("g++", "-O2", "-std=c++14", "-static", "-lm", "-m32", "-o",
			exec, "-xc++", source);
	case SolutionLanguage::PASCAL:
		return make("fpc", "-O2", "-XS", "-Xt", concat("-o", exec), source);
	case SolutionLanguage::UNKNOWN:
		THROW("Invalid Language!");
	}

	THROW("Should not reach here");
}

constexpr const char* sim::JudgeWorker::SOLUTION_FILENAME;
constexpr const char* sim::JudgeWorker::CHECKER_FILENAME;

int JudgeWorker::compile_impl(FilePath source, SolutionLanguage lang,
	Optional<std::chrono::nanoseconds> time_limit, string* c_errors,
	size_t c_errors_max_len, const string& proot_path,
	StringView compilation_source_basename, StringView exec_dest_filename)
{
	STACK_UNWINDING_MARK;

	auto compilation_dir = concat<PATH_MAX>(tmp_dir.path(), "compilation/");
	if (remove_r(compilation_dir) and errno != ENOENT)
		THROW("remove_r()", errmsg());

	if (mkdir(compilation_dir))
		THROW("mkdir()", errmsg());

	auto src_filename = concat<PATH_MAX>(compilation_source_basename);
	switch (lang) {
	case SolutionLanguage::C11:
		src_filename.append(".c");
		break;

	case SolutionLanguage::CPP11:
	case SolutionLanguage::CPP14:
		src_filename.append(".cpp");
		break;

	case SolutionLanguage::PASCAL:
		src_filename.append(".pas");
		break;

	case SolutionLanguage::UNKNOWN:
		THROW("Invalid language: ", (int)EnumVal<SolutionLanguage>(lang).int_val());
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


int JudgeWorker::compile_checker(Optional<std::chrono::nanoseconds> time_limit,
	std::string* c_errors, size_t c_errors_max_len,
	const std::string& proot_path)
{
	STACK_UNWINDING_MARK;

	auto checker_path_and_lang = [&]() -> std::pair<std::string, SolutionLanguage> {
		if (sf.checker.has_value()) {
			return {package_loader->load_as_file(sf.checker.value(), "checker"),
				filename_to_lang(sf.checker.value())};
		}

		auto path = concat_tostr(tmp_dir.path(), "default_checker.c");
		putFileContents(path, (const char*)default_checker_c,
			default_checker_c_len);

		return {path, SolutionLanguage::C};
	}();

	return compile_impl(checker_path_and_lang.first, checker_path_and_lang.second,
		time_limit, c_errors, c_errors_max_len, proot_path, "checker",
		CHECKER_FILENAME);
}

void JudgeWorker::load_package(FilePath package_path, Optional<string> simfile) {
	STACK_UNWINDING_MARK;

	if (isDirectory(package_path))
		package_loader = std::make_unique<DirPackageLoader>(package_path);
	else
		package_loader = std::make_unique<ZipPackageLoader>(tmp_dir, package_path);


	if (simfile.has_value())
		sf = Simfile(std::move(simfile.value()));
	else
		sf = Simfile(package_loader->load_as_str("Simfile"));

	sf.load_tests_with_files();
	sf.load_checker();
}

// Real time limit is set to 1.5 * time_limit + 1s, because CPU time is measured
static inline auto cpu_time_limit_to_real_time_limit(
	std::chrono::nanoseconds cpu_tl) noexcept
{
	return cpu_tl * 3 / 2 + std::chrono::seconds(1);
}

Sandbox::ExitStat JudgeWorker::run_solution(FilePath input_file,
	FilePath output_file, Optional<std::chrono::nanoseconds> time_limit,
	Optional<uint64_t> memory_limit) const
{
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (time_limit.has_value() and time_limit.value() <= 0ns)
		THROW("If set, time_limit has to be greater than 0");

	if (memory_limit.has_value() and memory_limit.value() <= 0)
		THROW("If set, memory_limit has to be greater than 0");

	// Solution STDOUT
	FileDescriptor solution_stdout(output_file, O_WRONLY | O_CREAT | O_TRUNC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", output_file, '`', errmsg());

	Sandbox sandbox;
	string solution_path(concat_tostr(tmp_dir.path(), SOLUTION_FILENAME));

	FileDescriptor test_in(input_file, O_RDONLY);
	if (test_in < 0)
		THROW("Failed to open file `", input_file, '`', errmsg());

	Optional<std::chrono::nanoseconds> real_time_limit;
	if (time_limit.has_value())
		real_time_limit = cpu_time_limit_to_real_time_limit(time_limit.value());

	// Run solution on the test
	Sandbox::ExitStat es = sandbox.run(solution_path, {}, {
		test_in,
		solution_stdout,
		-1,
		real_time_limit,
		memory_limit,
		time_limit
	}); // Allow exceptions to fly upper

	return es;
}

JudgeReport JudgeWorker::judge(bool final, JudgeLogger& judge_log) const {
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (checker_time_limit.has_value() and checker_time_limit.value() <= 0ns)
		THROW("If set, checker_time_limit has to be greater than 0");

	if (checker_memory_limit.has_value() and checker_memory_limit.value() <= 0)
		THROW("If set, checker_memory_limit has to be greater than 0");

	if (score_cut_lambda < 0 or score_cut_lambda > 1)
		THROW("score_cut_lambda has to be from [0, 1]");

	JudgeReport report;
	judge_log.begin(final);

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
		checker_time_limit,
		checker_memory_limit
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

		using std::chrono_literals::operator""s;

		for (auto&& test : group.tests) {
			// Prepare solution fds
			(void)ftruncate(solution_stdout, 0);
			(void)lseek(solution_stdout, 0, SEEK_SET);

			string test_in_path = package_loader->load_as_file(test.in, "test.in");
			string test_out_path = package_loader->load_as_file(test.out, "test.out");

			FileDescriptor test_in(test_in_path, O_RDONLY);
			if (test_in < 0)
				THROW("Failed to open file `", test_in_path, '`', errmsg());

			// Run solution on the test
			Sandbox::ExitStat es = sandbox.run(solution_path, {}, {
				test_in,
				solution_stdout,
				-1,
				cpu_time_limit_to_real_time_limit(test.time_limit),
				test.memory_limit,
				test.time_limit
			}); // Allow exceptions to fly upper

			report_group.tests.emplace_back(
				test.name,
				JudgeReport::Test::OK,
				es.cpu_runtime,
				test.time_limit,
				es.vm_peak,
				test.memory_limit,
				string {}
			);
			auto& test_report = report_group.tests.back();

			// Update score_ratio
			if (score_cut_lambda < 1) { // Only then the scaling occurs
				const double x =
					std::chrono::duration<double>(test_report.runtime).count();
				const double t =
					std::chrono::duration<double>(test_report.time_limit).count();
				score_ratio = std::min(score_ratio,
					(x / t - 1) / (score_cut_lambda - 1));
				// Runtime may be greater than time_limit therefore score_ratio
				// may become negative which is undesired
				if (score_ratio < 0)
					score_ratio = 0;

			}

			// OK
			if (es.si.code == CLD_EXITED and es.si.status == 0 and
				test_report.runtime <= test_report.time_limit)
			{

			// TLE
			} else if (test_report.runtime >= test_report.time_limit or
				es.runtime >= test.time_limit)
			{
				// es.runtime >= tl means that real_time_limit has been exceeded
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
			} else if (checker_time_limit.has_value() and
				ces.runtime >= checker_time_limit.value())
			{
				score_ratio = 0; // Do not give score for a checker error
				test_report.status = JudgeReport::Test::CHECKER_ERROR;
				test_report.comment = "Checker error";
				checker_error_str.append("Time limit exceeded");

			// Checker MLE
			} else if (ces.message == "Memory limit exceeded" or
				(checker_memory_limit.has_value() and
					ces.vm_peak > checker_memory_limit.value()))
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
			judge_log.test(test.name, test_report, es, ces,
				checker_memory_limit, checker_error_str);
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
