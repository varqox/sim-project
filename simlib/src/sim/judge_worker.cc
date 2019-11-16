#include "../../include/sim/judge_worker.hh"
#include "../../include/enum_val.hh"
#include "../../include/file_info.hh"
#include "../../include/libzip.hh"
#include "../../include/sim/checker.hh"
#include "../../include/sim/problem_package.hh"
#include "../../include/simple_parser.hh"
#include "../../include/unlinked_temporary_file.hh"
#include "default_checker_dump.h"

#include <cmath>
#include <future>
#include <thread>

using std::promise;
using std::string;
using std::thread;
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
		return get_file_contents(concat(pkg_root_, path));
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
	     pkg_master_dir_(sim::zip_package_master_dir(zip_)) {}

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

inline static vector<string>
compile_command(SolutionLanguage lang, StringView source, StringView exec) {
	STACK_UNWINDING_MARK;

	// Makes vector of strings from arguments
	auto make = [](auto... args) {
		vector<string> res;
		res.reserve(sizeof...(args));
		(void)std::initializer_list<int> {
		   (res.emplace_back(data(args), string_length(args)), 0)...};
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
	case SolutionLanguage::CPP17:
		return make("g++", "-O2", "-std=c++17", "-static", "-lm", "-m32", "-o",
		            exec, "-xc++", source);
	case SolutionLanguage::PASCAL:
		return make("fpc", "-O2", "-XS", "-Xt", concat("-o", exec), source);
	case SolutionLanguage::UNKNOWN: THROW("Invalid Language!");
	}

	THROW("Should not reach here");
}

int JudgeWorker::compile_impl(
   FilePath source, SolutionLanguage lang,
   std::optional<std::chrono::nanoseconds> time_limit, string* c_errors,
   size_t c_errors_max_len, const string& proot_path,
   StringView compilation_source_basename, StringView exec_dest_filename) {
	STACK_UNWINDING_MARK;

	auto compilation_dir = concat<PATH_MAX>(tmp_dir.path(), "compilation/");
	if (remove_r(compilation_dir) and errno != ENOENT)
		THROW("remove_r()", errmsg());

	if (mkdir(compilation_dir))
		THROW("mkdir()", errmsg());

	auto src_filename = concat<PATH_MAX>(compilation_source_basename);
	switch (lang) {
	case SolutionLanguage::C11: src_filename.append(".c"); break;

	case SolutionLanguage::CPP11:
	case SolutionLanguage::CPP14:
	case SolutionLanguage::CPP17: src_filename.append(".cpp"); break;

	case SolutionLanguage::PASCAL: src_filename.append(".pas"); break;

	case SolutionLanguage::UNKNOWN:
		THROW("Invalid language: ",
		      (int)EnumVal<SolutionLanguage>(lang).int_val());
	}

	if (copy(source, concat<PATH_MAX>(compilation_dir, src_filename)))
		THROW("copy()", errmsg());

	int rc = compile(compilation_dir,
	                 compile_command(lang, src_filename, exec_dest_filename),
	                 time_limit, c_errors, c_errors_max_len, proot_path);

	if (rc == 0 and
	    move(concat<PATH_MAX>(compilation_dir, exec_dest_filename),
	         concat<PATH_MAX>(tmp_dir.path(), exec_dest_filename))) {
		THROW("move()", errmsg());
	}

	return rc;
}

int JudgeWorker::compile_checker(
   std::optional<std::chrono::nanoseconds> time_limit, std::string* c_errors,
   size_t c_errors_max_len, const std::string& proot_path) {
	STACK_UNWINDING_MARK;

	auto checker_path_and_lang =
	   [&]() -> std::pair<std::string, SolutionLanguage> {
		if (sf.checker.has_value()) {
			return {package_loader->load_as_file(sf.checker.value(), "checker"),
			        filename_to_lang(sf.checker.value())};
		}

		auto path = concat_tostr(tmp_dir.path(), "default_checker.c");
		put_file_contents(path, (const char*)default_checker_c,
		                  default_checker_c_len);

		return {path, SolutionLanguage::C};
	}();

	return compile_impl(
	   checker_path_and_lang.first, checker_path_and_lang.second, time_limit,
	   c_errors, c_errors_max_len, proot_path, "checker", CHECKER_FILENAME);
}

void JudgeWorker::load_package(FilePath package_path,
                               std::optional<string> simfile) {
	STACK_UNWINDING_MARK;

	if (is_directory(package_path)) {
		package_loader = std::make_unique<DirPackageLoader>(package_path);
	} else {
		package_loader =
		   std::make_unique<ZipPackageLoader>(tmp_dir, package_path);
	}

	if (simfile.has_value())
		sf = Simfile(std::move(simfile.value()));
	else
		sf = Simfile(package_loader->load_as_str("Simfile"));

	sf.load_tests_with_files();
	sf.load_checker();
}

// Real time limit is set to 1.5 * time_limit + 0.5s, because CPU time is
// measured
static inline auto
cpu_time_limit_to_real_time_limit(std::chrono::nanoseconds cpu_tl) noexcept {
	return cpu_tl * 3 / 2 + std::chrono::milliseconds(500);
}

Sandbox::ExitStat
JudgeWorker::run_solution(FilePath input_file, FilePath output_file,
                          std::optional<std::chrono::nanoseconds> time_limit,
                          std::optional<uint64_t> memory_limit) const {
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (time_limit.has_value() and time_limit.value() <= 0ns)
		THROW("If set, time_limit has to be greater than 0");

	if (memory_limit.has_value() and memory_limit.value() <= 0)
		THROW("If set, memory_limit has to be greater than 0");

	// Solution STDOUT
	FileDescriptor solution_stdout(output_file,
	                               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", output_file, '`', errmsg());

	Sandbox sandbox;
	string solution_path(concat_tostr(tmp_dir.path(), SOLUTION_FILENAME));

	FileDescriptor test_in(input_file, O_RDONLY | O_CLOEXEC);
	if (test_in < 0)
		THROW("Failed to open file `", input_file, '`', errmsg());

	std::optional<std::chrono::nanoseconds> real_time_limit;
	if (time_limit.has_value())
		real_time_limit = cpu_time_limit_to_real_time_limit(time_limit.value());

	// Run solution on the test
	Sandbox::ExitStat es =
	   sandbox.run(solution_path, {},
	               {test_in, solution_stdout, -1, real_time_limit, memory_limit,
	                time_limit}); // Allow exceptions to fly upper

	return es;
}

namespace {

struct CheckerStatus {
	enum { OK, WRONG, ERROR } status;
	double ratio;
	std::string message;
};

} // namespace

static CheckerStatus exit_to_checker_status(const Sandbox::ExitStat& es,
                                            const Sandbox::Options& opts,
                                            int output_fd,
                                            const char* output_name) {
	// Checker exited with 0
	if (es.si.code == CLD_EXITED and es.si.status == 0) {
		auto chout = sim::obtain_checker_output(output_fd, 512);
		SimpleParser parser(chout);

		StringView line1(parser.extract_next('\n')); // "OK" or "WRONG"
		StringView line2(parser.extract_next('\n')); // percentage (real)

		auto wrong_second_line = [&]() -> CheckerStatus {
			return {
			   CheckerStatus::ERROR, 0,
			   concat_tostr(
			      "Second line of the ", output_name, " is invalid: `", line2,
			      "` - it has to be either empty or a real number representing "
			      "the percentage of the score that solution will receive")};
		};

		// Second line has to be either empty or be a real number
		if (line2.size() and (line2[0] == '-' or !is_real(line2))) {
			wrong_second_line();
		} else if (line1 == "OK") { // "OK" -> Checker: OK
			CheckerStatus res;
			// line2 format was checked above
			if (line2.empty()) {
				res.ratio = 1; // Empty line means 100%
			} else {
				errno = 0;
				char* ptr;
				double x = strtod(line2.data(), &ptr);
				if (errno != 0 or ptr == line2.data())
					return wrong_second_line();

				res.ratio = x * 0.01;
			}

			res.status = CheckerStatus::OK;
			// Leave the checker comment only
			chout.erase(chout.begin(), chout.end() - parser.size());
			res.message = std::move(chout);
			return res;
		}

		// "WRONG" -> Checker: WA
		if (line1 == "WRONG") {
			// Leave the checker comment only
			chout.erase(chout.begin(), chout.end() - parser.size());
			return {CheckerStatus::WRONG, 0, std::move(chout)};
		}

		// * -> Checker error
		return {CheckerStatus::ERROR, 0,
		        concat_tostr("First line of the ", output_name,
		                     " is invalid: `", line1,
		                     "` - it has to be either `OK` or `WRONG`")};
	}

	// Checker TLE
	if (opts.real_time_limit.has_value() and
	    es.runtime >= opts.real_time_limit.value()) {
		return {CheckerStatus::ERROR, 0, "Time limit exceeded"};
	}

	// Checker MLE
	if (es.message == "Memory limit exceeded" or
	    (opts.memory_limit.has_value() and
	     es.vm_peak > opts.memory_limit.value())) {
		return {CheckerStatus::ERROR, 0, "Memory limit exceeded"};
	}

	// Checker RTE
	CheckerStatus res {CheckerStatus::ERROR, 0, "Runtime error"};
	if (es.message.size())
		back_insert(res.message, " (", es.message, ')');
	return res;
}

template <class Func>
JudgeReport JudgeWorker::process_tests(
   bool final, JudgeLogger& judge_log,
   const std::optional<std::function<void(const JudgeReport&)>>&
      partial_report_callback,
   Func&& judge_on_test) const {
	using std::chrono_literals::operator""s;

	JudgeReport report;
	judge_log.begin(final);

	// First round - judge as little as possible to compute total score
	bool test_were_skipped = false;
	uint64_t total_score = 0, max_score = 0;
	for (auto const& group : sf.tgroups) {
		// Group "0" goes to the initial report, others groups to final
		auto p = Simfile::TestNameComparator::split(group.tests[0].name);
		if ((p.gid != "0") != final)
			continue;

		report.groups.emplace_back();
		auto& report_group = report.groups.back();

		double group_score_ratio = 1.0;
		auto calc_group_score = [&] {
			return (int64_t)round(group.score * group_score_ratio);
		};

		bool skip_tests = false;
		for (auto const& test : group.tests) {
			if (skip_tests) {
				test_were_skipped = true;
				report_group.tests.emplace_back(
				   test.name, JudgeReport::Test::SKIPPED, 0s, test.time_limit,
				   0, test.memory_limit, string {});
			} else {
				report_group.tests.emplace_back(
				   judge_on_test(test, group_score_ratio));

				// Update group_score_ratio
				if (score_cut_lambda < 1) { // Only then the scaling occurs
					const auto& test_report = report_group.tests.back();
					const double x =
					   std::chrono::duration<double>(test_report.runtime)
					      .count();
					const double t =
					   std::chrono::duration<double>(test_report.time_limit)
					      .count();
					group_score_ratio = std::min(
					   group_score_ratio, (x / t - 1) / (score_cut_lambda - 1));
					// Runtime may be greater than time_limit therefore
					// group_score_ratio may become negative which is undesired
					if (group_score_ratio < 0)
						group_score_ratio = 0;
				}

				if (group_score_ratio < 1e-6 and calc_group_score() == 0)
					skip_tests = partial_report_callback.has_value();
			}
		}

		// Compute group score
		report_group.score = calc_group_score();
		report_group.max_score = group.score;

		total_score += report_group.score;
		if (report_group.max_score > 0)
			max_score += report_group.max_score;

		judge_log.group_score(report_group.score, report_group.max_score,
		                      group_score_ratio);
	}

	judge_log.final_score(total_score, max_score);

	if (test_were_skipped) {
		report.judge_log = judge_log.judge_log();
		partial_report_callback.value()(report);

		// Second round - judge remaining tests
		for (size_t gi = 0, rgi = 0; gi < sf.tgroups.size(); ++gi) {
			auto const& group = sf.tgroups[gi];

			// Group "0" goes to the initial report, others groups to final
			auto p = Simfile::TestNameComparator::split(group.tests[0].name);
			if ((p.gid != "0") != final)
				continue;

			auto& report_group = report.groups[rgi++];

			double group_score_ratio = 0; // Remaining tests have ratio == 0
			for (size_t ti = 0; ti < group.tests.size(); ++ti) {
				auto const& test = group.tests[ti];
				auto& test_report = report_group.tests[ti];
				if (test_report.status == JudgeReport::Test::SKIPPED)
					test_report = judge_on_test(test, group_score_ratio);
			}
		}
	}

	judge_log.end();
	report.judge_log = judge_log.judge_log();
	return report;
}

JudgeReport JudgeWorker::judge_interactive(
   bool final, JudgeLogger& judge_log,
   const std::optional<std::function<void(const JudgeReport&)>>&
      partial_report_callback) const {
	STACK_UNWINDING_MARK;

	struct NextJob {
		FileDescriptor checker_stdin;
		FileDescriptor checker_stdout;
		const string& test_in_path;
		std::chrono::nanoseconds solution_real_time_limit;
	};

	std::promise<void> checker_supervisor_ready; // Used to pass exceptions
	std::promise<std::optional<NextJob>> next_job_promise;
	std::promise<CheckerStatus> checker_status_promise;
	Sandbox::ExitStat ces;

	auto checker_supervisor = [&] {
		try {
			STACK_UNWINDING_MARK;

			// Setup
			FileDescriptor output(open_unlinked_tmp_file());
			if (output < 0) // Needs to be done this way because constructing
			                // exception may throw
				THROW("Cannot create checker output file descriptor");

			Sandbox sandbox;
			const auto checker_path =
			   concat_tostr(tmp_dir.path(), CHECKER_FILENAME);

			checker_supervisor_ready.set_value();
			for (;;) {
				STACK_UNWINDING_MARK;

				auto job_opt = next_job_promise.get_future().get();
				next_job_promise = {}; // reset promise
				checker_supervisor_ready
				   .set_value(); // Notify solution supervisor that we received
				                 // the job
				if (not job_opt.has_value())
					return;

				auto& job = job_opt.value();

				try {
					STACK_UNWINDING_MARK;

					auto rtl = checker_time_limit;
					if (rtl.has_value())
						rtl.value() += job.solution_real_time_limit;

					// Checker parameters
					Sandbox::Options opts = {job.checker_stdin,
					                         job.checker_stdout,
					                         output, // STDERR
					                         rtl, checker_memory_limit};

					// Prepare checker fds
					(void)ftruncate(output, 0);
					(void)lseek(output, 0, SEEK_SET);

					// Run checker
					ces = sandbox.run(
					   checker_path, {checker_path, job.test_in_path}, opts,
					   {{job.test_in_path, OpenAccess::RDONLY}}, [&] {
						   (void)job.checker_stdin.close();
						   (void)job.checker_stdout.close();
					   });

					// Make sure the pipes are closed
					(void)job.checker_stdin.close();
					(void)job.checker_stdout.close();

					checker_status_promise.set_value(
					   exit_to_checker_status(ces, opts, output, "stderr"));

				} catch (...) {
					ERRLOG_CATCH();
					checker_status_promise.set_exception(
					   std::current_exception());
				}
			}
		} catch (...) {
			ERRLOG_CATCH();
			checker_supervisor_ready.set_exception(std::current_exception());
		}
	};

	Sandbox sandbox;
	const auto solution_path = concat_tostr(tmp_dir.path(), SOLUTION_FILENAME);

	auto judge_on_test = [&](const sim::Simfile::Test& test,
	                         double& group_score_ratio) {
		STACK_UNWINDING_MARK;
		// Prepare pipes
		int pfds[2];
		if (pipe2(pfds, O_CLOEXEC))
			THROW("pipe2()", errmsg());
		FileDescriptor checker_input(pfds[0]), solution_output(pfds[1]);

		if (pipe2(pfds, O_CLOEXEC))
			THROW("pipe2()", errmsg());
		FileDescriptor solution_input(pfds[0]), checker_output(pfds[1]);

		string test_in_path = package_loader->load_as_file(test.in, "test.in");
		auto solution_real_time_limit =
		   cpu_time_limit_to_real_time_limit(test.time_limit);

		// Schedule checker supervisor
		next_job_promise.set_value(
		   NextJob {std::move(checker_input), std::move(checker_output),
		            test_in_path, solution_real_time_limit});
		// Synchronize with checker supervisor to safely recover later in case
		// of exceptions (need to set next_job_promise to std::nullopt)
		checker_supervisor_ready.get_future().get();
		checker_supervisor_ready = {}; // reset promise

		// Run solution
		auto es = sandbox.run(solution_path, {},
		                      {solution_input, solution_output, -1,
		                       solution_real_time_limit, test.memory_limit,
		                       test.time_limit},
		                      {}, [&] {
			                      (void)solution_input.close();
			                      (void)solution_output.close();
		                      }); // Allow exceptions to fly upper

		// Make sure the pipes are closed
		(void)solution_input.close();
		(void)solution_output.close();

		// Get checker status
		auto checker_result = checker_status_promise.get_future().get();
		checker_status_promise = {}; // reset promise

		JudgeReport::Test test_report(test.name, JudgeReport::Test::OK,
		                              es.cpu_runtime, test.time_limit,
		                              es.vm_peak, test.memory_limit, string {});

		// Update group_score_ratio
		group_score_ratio = std::min(group_score_ratio, checker_result.ratio);

		switch (checker_result.status) {
		case CheckerStatus::ERROR:
			test_report.status = JudgeReport::Test::CHECKER_ERROR;
			test_report.comment = "Checker error";
			judge_log.test(test.name, test_report, es, ces,
			               checker_memory_limit, checker_result.message);
			return test_report;

		case CheckerStatus::OK:
		case CheckerStatus::WRONG:
			break; // This way to make the compiler warning on development
		}

		// Solution: OK or killed by SIGPIPE and checker returned WRONG
		if ((es.si.code == CLD_EXITED and es.si.status == 0 and
		     test_report.runtime <= test_report.time_limit) or
		    (is_one_of(es.si.code, CLD_KILLED, CLD_DUMPED) and
		     es.si.status == SIGPIPE and
		     checker_result.status == CheckerStatus::WRONG)) {
			switch (checker_result.status) {
			case CheckerStatus::OK:
				// Test status is already set to OK
				test_report.comment = std::move(checker_result.message);
				break;

			case CheckerStatus::WRONG:
				test_report.status = JudgeReport::Test::WA;
				test_report.comment = std::move(checker_result.message);
				break;

			case CheckerStatus::ERROR:
				throw_assert(false); // This way to make the compiler warning
				                     // on development
			}

			judge_log.test(test.name, test_report, es, ces,
			               checker_memory_limit, checker_result.message);
			return test_report;
		}

		group_score_ratio = 0; // Other statuses are fatal

		// After checking status for OK >= comparison is safe to detect
		// exceeding
		if (test_report.runtime >= test_report.time_limit or
		    es.runtime >= test.time_limit) {
			// Solution: TLE
			// es.runtime >= tl meas that real_time_limit has been exceeded
			if (test_report.runtime < test_report.time_limit)
				test_report.runtime = test_report.time_limit;

			test_report.status = JudgeReport::Test::TLE;
			test_report.comment = "Time limit exceeded";
		} else if (es.message == "Memory limit exceeded" or
		           test_report.memory_consumed > test_report.memory_limit) {
			// Solution: MLE
			test_report.status = JudgeReport::Test::MLE;
			test_report.comment = "Memory limit exceeded";
		} else {
			// Solution: RTE
			test_report.status = JudgeReport::Test::RTE;
			test_report.comment = "Runtime error";
			if (es.message.size())
				back_insert(test_report.comment, " (", es.message, ')');
		}

		// Logging
		judge_log.test(test.name, test_report, es);
		return test_report;
	};

	auto solution_supervisor = [&] {
		STACK_UNWINDING_MARK;
		// Wait for checker supervisor to set up
		checker_supervisor_ready.get_future().get();
		checker_supervisor_ready = {}; // reset promise

		return process_tests(final, judge_log, partial_report_callback,
		                     judge_on_test);
	};

	std::thread checker_supervisor_thread(checker_supervisor);
	try {
		// This thread is a solution supervisor thread
		auto report = solution_supervisor();
		next_job_promise.set_value(std::nullopt); // No more jobs
		checker_supervisor_thread.join();
		return report;

	} catch (...) {
		next_job_promise.set_value(std::nullopt);
		if (checker_supervisor_thread.joinable()) // Join above may have thrown
			checker_supervisor_thread.join();
		throw;
	}
}

JudgeReport
JudgeWorker::judge(bool final, JudgeLogger& judge_log,
                   const std::optional<std::function<void(const JudgeReport&)>>&
                      partial_report_callback) const {
	STACK_UNWINDING_MARK;

	using std::chrono_literals::operator""ns;

	if (checker_time_limit.has_value() and checker_time_limit.value() <= 0ns)
		THROW("If set, checker_time_limit has to be greater than 0");

	if (checker_memory_limit.has_value() and checker_memory_limit.value() <= 0)
		THROW("If set, checker_memory_limit has to be greater than 0");

	if (score_cut_lambda < 0 or score_cut_lambda > 1)
		THROW("score_cut_lambda has to be from [0, 1]");

	if (sf.interactive)
		return judge_interactive(final, judge_log,
		                         std::move(partial_report_callback));

	string sol_stdout_path {tmp_dir.path() + "sol_stdout"};
	// Checker STDOUT
	FileDescriptor checker_stdout {open_unlinked_tmp_file(O_CLOEXEC)};
	if (checker_stdout < 0)
		THROW("Failed to create unlinked temporary file", errmsg());

	// Solution STDOUT
	FileDescriptor solution_stdout(sol_stdout_path,
	                               O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
	if (solution_stdout < 0)
		THROW("Failed to open file `", sol_stdout_path, '`', errmsg());

	FileRemover solution_stdout_remover {sol_stdout_path}; // Save disk space

	// Checker parameters
	Sandbox::Options checker_opts = {-1, // STDIN is ignored
	                                 checker_stdout, // STDOUT
	                                 -1, // STDERR is ignored
	                                 checker_time_limit, checker_memory_limit};

	Sandbox sandbox;

	string checker_path {concat_tostr(tmp_dir.path(), CHECKER_FILENAME)};
	string solution_path {concat_tostr(tmp_dir.path(), SOLUTION_FILENAME)};

	using std::chrono_literals::operator""s;

	auto judge_on_test = [&](const sim::Simfile::Test& test,
	                         double& group_score_ratio) {
		STACK_UNWINDING_MARK;

		// Prepare solution fds
		(void)ftruncate(solution_stdout, 0);
		(void)lseek(solution_stdout, 0, SEEK_SET);

		string test_in_path = package_loader->load_as_file(test.in, "test.in");
		string test_out_path =
		   package_loader->load_as_file(test.out.value(), "test.out");

		FileDescriptor test_in(test_in_path, O_RDONLY | O_CLOEXEC);
		if (test_in < 0)
			THROW("Failed to open file `", test_in_path, '`', errmsg());

		// Run solution on the test
		Sandbox::ExitStat es =
		   sandbox.run(solution_path, {},
		               {test_in, solution_stdout, -1,
		                cpu_time_limit_to_real_time_limit(test.time_limit),
		                test.memory_limit,
		                test.time_limit}); // Allow exceptions to fly upper

		JudgeReport::Test test_report(test.name, JudgeReport::Test::OK,
		                              es.cpu_runtime, test.time_limit,
		                              es.vm_peak, test.memory_limit, string {});

		if (es.si.code == CLD_EXITED and es.si.status == 0 and
		    test_report.runtime <= test_report.time_limit) {
			// OK

		} else if (test_report.runtime >= test_report.time_limit or
		           es.runtime >= test.time_limit) { // After checking status for
			                                        // OK `>=` comparison is
			                                        // safe to detect exceeding
			// TLE
			// es.runtime >= tl means that real_time_limit has been exceeded
			if (test_report.runtime < test_report.time_limit)
				test_report.runtime = test_report.time_limit;

			group_score_ratio = 0;
			test_report.status = JudgeReport::Test::TLE;
			test_report.comment = "Time limit exceeded";
			judge_log.test(test.name, test_report, es);
			return test_report;

		} else if (es.message == "Memory limit exceeded" or
		           test_report.memory_consumed > test_report.memory_limit) {
			// MLE
			group_score_ratio = 0;
			test_report.status = JudgeReport::Test::MLE;
			test_report.comment = "Memory limit exceeded";
			judge_log.test(test.name, test_report, es);
			return test_report;

		} else {
			// RTE
			group_score_ratio = 0;
			test_report.status = JudgeReport::Test::RTE;
			test_report.comment = "Runtime error";
			if (es.message.size())
				back_insert(test_report.comment, " (", es.message, ')');

			judge_log.test(test.name, test_report, es);
			return test_report;
		}

		// Status == OK

		/* Checking solution output with checker */

		// Prepare checker fds
		(void)ftruncate(checker_stdout, 0);
		(void)lseek(checker_stdout, 0, SEEK_SET);

		// Run checker
		auto ces = sandbox.run(
		   checker_path,
		   {checker_path, test_in_path, test_out_path, sol_stdout_path},
		   checker_opts,
		   {{test_in_path, OpenAccess::RDONLY},
		    {test_out_path, OpenAccess::RDONLY},
		    {sol_stdout_path,
		     OpenAccess::RDONLY}}); // Allow exceptions to fly higher

		auto checker_result =
		   exit_to_checker_status(ces, checker_opts, checker_stdout, "stdout");

		// Update group_score_ratio
		group_score_ratio = std::min(group_score_ratio, checker_result.ratio);

		switch (checker_result.status) {
		case CheckerStatus::OK:
			// Test status is already set to OK
			test_report.comment = std::move(checker_result.message);
			break;
		case CheckerStatus::WRONG:
			test_report.status = JudgeReport::Test::WA;
			test_report.comment = std::move(checker_result.message);
			break;
		case CheckerStatus::ERROR:
			test_report.status = JudgeReport::Test::CHECKER_ERROR;
			test_report.comment = "Checker error";
			break;
		}

		// Logging
		judge_log.test(test.name, test_report, es, ces, checker_memory_limit,
		               checker_result.message);

		return test_report;
	};

	return process_tests(final, judge_log, partial_report_callback,
	                     judge_on_test);
}

} // namespace sim
