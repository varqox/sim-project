#include "../include/sim/conver.h"
#include "../include/avl_dict.h"
#include "../include/concurrent/job_processor.h"
#include "../include/libzip.h"
#include "../include/process.h"

#include <gtest/gtest.h>

using sim::Conver;
using sim::JudgeReport;
using sim::JudgeWorker;
using std::string;

static Conver::Options load_options_from_file(FilePath file) {
	ConfigFile cf;
	cf.load_config_from_file(file, true);

	auto get_var = [&](StringView name) -> decltype(auto) {
		auto const& var = cf[name];
		if (not var.is_set())
			THROW("Variable \"", name, "\" is not set");
		if (var.is_array())
			THROW("Variable \"", name, "\" is an array");
		return var;
	};

	auto get_string = [&](StringView name) -> decltype(auto) {
		return get_var(name).as_string();
	};

	auto get_uint64 = [&](StringView name) {
		return get_var(name).as_int<uint64_t>();
	};

	auto get_optional_uint64 = [&](StringView name) -> std::optional<uint64_t> {
		if (get_var(name).as_string() == "null")
			return std::nullopt;

		return get_uint64(name);
	};

	auto get_double = [&](StringView name) {
		return get_var(name).as_double();
	};

	auto get_duration = [&](StringView name) {
		using namespace std::chrono;
		return duration_cast<nanoseconds>(duration<double>(get_double(name)));
	};

	auto get_optional_duration =
	   [&](StringView name) -> std::optional<std::chrono::nanoseconds> {
		if (get_var(name).as_string() == "null")
			return std::nullopt;

		return get_duration(name);
	};

	auto get_bool = [&](StringView name) {
		StringView str = get_string(name);
		if (str == "true")
			return true;
		if (str == "false")
			return false;
		THROW("variable \"", name, "\" is not a bool: ", str);
	};

	auto get_optional_bool = [&](StringView name) -> std::optional<bool> {
		if (get_var(name).as_string() == "null")
			return std::nullopt;

		return get_bool(name);
	};

	Conver::Options opts;

	opts.name = get_string("name");
	opts.label = get_string("label");
	opts.interactive = get_optional_bool("interactive");
	opts.memory_limit = get_optional_uint64("memory_limit");
	opts.global_time_limit = get_optional_duration("global_time_limit");
	opts.max_time_limit = get_duration("max_time_limit");
	opts.reset_time_limits_using_model_solution =
	   get_bool("reset_time_limits_using_model_solution");
	opts.ignore_simfile = get_bool("ignore_simfile");
	opts.seek_for_new_tests = get_bool("seek_for_new_tests");
	opts.reset_scoring = get_bool("reset_scoring");
	opts.require_statement = get_bool("require_statement");
	opts.rtl_opts.min_time_limit = get_duration("min_time_limit");
	opts.rtl_opts.solution_runtime_coefficient =
	   get_double("solution_rutnime_coefficient");

	return opts;
}

class TestingJudgeLogger : public sim::JudgeLogger {
	template<class... Args>
	void log(Args&&... args) {
		back_insert(log_, std::forward<Args>(args)...);
	}

	template <class Func>
	void log_test(StringView test_name, JudgeReport::Test test_report,
	              Sandbox::ExitStat es, Func&& func) {
		log("  ", paddedString(test_name, 8, LEFT), ' ',
		       " [ TL: ", toString(floor_to_10ms(test_report.time_limit), false),
		       " s ML: ", test_report.memory_limit >> 10, " KiB ]  Status: ",
		       JudgeReport::simple_span_status(test_report.status));

		if (test_report.status == JudgeReport::Test::RTE)
			log(" (", es.message, ')');

		func();

		log('\n');
	}

public:
	TestingJudgeLogger() = default;

	void begin(bool final) override {
		log_.clear();
		log("Judging (", (final ? "final" : "initial"), "): {\n");
	}

	void test(StringView test_name, JudgeReport::Test test_report,
	          Sandbox::ExitStat es) override {
		log_test(test_name, test_report, es, [] {});
	}

	void test(StringView test_name, JudgeReport::Test test_report,
	          Sandbox::ExitStat es, Sandbox::ExitStat,
	          std::optional<uint64_t> checker_mem_limit,
	          StringView checker_error_str) override {
		log_test(test_name, test_report, es, [&]() {
			log("  Checker: ");

			// Checker status
			if (test_report.status == JudgeReport::Test::OK) {
				log("OK: ", test_report.comment);
			} else if (test_report.status == JudgeReport::Test::WA) {
				log("WA: ", test_report.comment);
			} else if (test_report.status == JudgeReport::Test::CHECKER_ERROR) {
				log("ERROR: ", checker_error_str);
			} else {
				return; // Checker was not run
			}

			if (checker_mem_limit.has_value())
				log(" [ ML: ", checker_mem_limit.value() >> 10, " KiB ]");
		});
	}

	void group_score(int64_t score, int64_t max_score,
	                 double score_ratio) override {
		log("Score: ", score, " / ", max_score,
		    " (ratio: ", toStr(score_ratio, 4), ")\n");
	}

	void final_score(int64_t, int64_t) override {
	}

	void end() override { log("}\n"); }
};

class ConverTestRunner : public concurrent::JobProcessor<string> {
	constexpr static bool REGENERATE_OUTS = false;
	const string tests_dir_;

public:
	ConverTestRunner(string tests_dir) : tests_dir_(std::move(tests_dir)) {}

protected:
	void produce_jobs() override final {
		std::vector<string> test_cases;
		collect_available_test_cases(test_cases);
		sort(test_cases.begin(), test_cases.end(), StrNumCompare());
		for (auto& test_case_name : test_cases)
			add_job(std::move(test_case_name));
	}

private:
	void collect_available_test_cases(std::vector<string>& test_cases) {
		forEachDirComponent(tests_dir_, [&](dirent* file) {
			constexpr StringView suffix("package.zip");
			StringView file_name(file->d_name);
			if (hasSuffix(file_name, suffix)) {
				file_name.removeSuffix(suffix.size());
				test_cases.emplace_back(file_name.to_string());
			}
		});
	}

protected:
	void process_job(string test_case_name) override final {
		try {
			run_test_case(std::move(test_case_name));
		} catch (const std::exception& e) {
			FAIL() << "Unexpected exception -> " << e.what();
		}
	}

private:
	void run_test_case(string&& test_case_name) const {
		stdlog("Running test case: ", test_case_name);
		TestCaseRunner(tests_dir_, std::move(test_case_name)).run();
	}

	class TestCaseRunner {
		static constexpr std::chrono::seconds COMPILATION_TIME_LIMIT {5};
		static constexpr size_t COMPILATION_ERRORS_MAX_LENGTH = 4096;

		const TemporaryFile package_copy_ {"/tmp/conver_test.XXXXXX"};
		const string test_case_name_;
		const InplaceBuff<PATH_MAX> test_path_prefix_;
		const Conver::Options options_;

		Conver conver_;
		string report_;
		sim::Simfile pre_simfile_;
		sim::Simfile post_simfile_;
		JudgeReport initial_judge_report, final_judge_report;

	public:
		TestCaseRunner(const string& tests_dir, string&& test_case_name)
		   : test_case_name_(std::move(test_case_name)), test_path_prefix_(concat(tests_dir, test_case_name_)),
		     options_(load_options_from_file(
		        concat_tostr(test_path_prefix_, "conver.options"))) {
			copy(concat_tostr(test_path_prefix_, "package.zip"),
			     package_copy_.path());
			conver_.package_path(package_copy_.path());
		}

		void run() {
			generate_result();
			check_result();
		}

	private:
		void generate_result() {
			try {
				auto cres = construct_simfiles();
				switch (cres.status) {
				case Conver::Status::COMPLETE: break;
				case Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT:
					judge_model_solution_and_finish_constructing_post_simfile(
					   std::move(cres));
					break;
				}
			} catch (const std::exception& e) {
				report_ = conver_.report();
				back_insert(report_, "\n>>>> Exception caught <<<<\n",
				            e.what());
			}
		}

		sim::Conver::ConstructionResult construct_simfiles() {
			auto cres = conver_.construct_simfile(options_);
			pre_simfile_ = cres.simfile;
			post_simfile_ = cres.simfile;
			report_ = conver_.report();

			return cres;
		}

		void judge_model_solution_and_finish_constructing_post_simfile(
		   sim::Conver::ConstructionResult cres) {
			ModelSolutionRunner model_solution_runner(
			   package_copy_.path(), post_simfile_, cres.pkg_master_dir);
			std::tie(initial_judge_report, final_judge_report) = model_solution_runner.judge();

			Conver::reset_time_limits_using_jugde_reports(
			   post_simfile_, initial_judge_report, final_judge_report, options_.rtl_opts);
		}

		class ModelSolutionRunner {
			JudgeWorker jworker_;
			sim::Simfile& simfile_;
			const string& package_path_;
			const string& pkg_master_dir_;

		public:
			ModelSolutionRunner(const string& package_path,
			                    sim::Simfile& simfile,
			                    const string& pkg_master_dir)
			   : simfile_(simfile), package_path_(package_path),
			     pkg_master_dir_(pkg_master_dir) {
				jworker_.load_package(package_path_, simfile_.dump());
			}

			// Returns (initial report, final report)
			std::pair<JudgeReport, JudgeReport> judge() {
				compile_checker();
				compile_solution(extract_solution());

				TestingJudgeLogger judge_logger;
				return {jworker_.judge(false, judge_logger), jworker_.judge(true, judge_logger)};
			}

		private:
			void compile_checker() {
				string compilation_errors;
				if (jworker_.compile_checker(
				       COMPILATION_TIME_LIMIT, &compilation_errors,
				       COMPILATION_ERRORS_MAX_LENGTH, "")) {
					THROW("failed to compile checker: \n", compilation_errors);
				}
			}

			OpenedTemporaryFile extract_solution() {
				OpenedTemporaryFile solution_source_code(
				   "/tmp/problem_solution.XXXXXX");
				ZipFile zip(package_path_);
				zip.extract_to_fd(zip.get_index(concat(pkg_master_dir_,
				                                       simfile_.solutions[0])),
				                  solution_source_code);

				return solution_source_code;
			}

			void compile_solution(OpenedTemporaryFile&& solution_source_code) {
				string compilation_errors;
				if (jworker_.compile_solution(
				       solution_source_code.path(),
				       sim::filename_to_lang(simfile_.solutions[0]),
				       COMPILATION_TIME_LIMIT, &compilation_errors,
				       COMPILATION_ERRORS_MAX_LENGTH, "")) {
					THROW("failed to compile solution: \n", compilation_errors);
				}
			}
		};

		void check_result() {
			round_post_simfile_time_limits_to_whole_seconds();
			if (REGENERATE_OUTS)
				overwrite_test_out_files();

			check_result_with_out_files();
		}

		void round_post_simfile_time_limits_to_whole_seconds() {
			using std::chrono_literals::operator""s;
			// This should remove the problem with random time limit if they
			// were set using the model solution.
			for (auto& group : post_simfile_.tgroups) {
				for (auto& test : group.tests) {
			        // Time limits should not have been set to 0
					EXPECT_GT(test.time_limit, 0s) << "^ test " << test_case_name_;
					test.time_limit =
					   std::chrono::duration_cast<std::chrono::seconds>(
					      test.time_limit + 0.5s);
				}
			}
		}

		void overwrite_test_out_files() const {
			overwrite_pre_simfile_out();
			overwrite_post_simfile_out();
			overwrite_conver_log_out();
			overwrite_judge_log_out();
		}

		void overwrite_pre_simfile_out() const {
			putFileContents(concat_tostr(test_path_prefix_, "pre_simfile.out"),
			                intentionalUnsafeStringView(pre_simfile_.dump()));
		}

		void overwrite_post_simfile_out() const {
			putFileContents(concat_tostr(test_path_prefix_, "post_simfile.out"),
			                intentionalUnsafeStringView(post_simfile_.dump()));
		}

		void overwrite_conver_log_out() const {
			putFileContents(concat_tostr(test_path_prefix_, "conver_log.out"),
			                report_);
		}

		void overwrite_judge_log_out() const {
			putFileContents(concat_tostr(test_path_prefix_, "judge_log.out"),
			                intentionalUnsafeStringView(serialized_judge_log()));
		}

		string serialized_judge_log() const {
			return initial_judge_report.judge_log + final_judge_report.judge_log;
		}

		void check_result_with_out_files() const {
			check_result_with_pre_simfile_out();
			check_result_with_post_simfile_out();
			check_result_with_conver_log_out();
			check_result_with_judge_log_out();
		}

		void check_result_with_pre_simfile_out() const {
			EXPECT_EQ(getFileContents(
			             concat_tostr(test_path_prefix_, "pre_simfile.out")),
			          pre_simfile_.dump()) << "^ test " << test_case_name_;
		}

		void check_result_with_post_simfile_out() const {
			EXPECT_EQ(getFileContents(
			             concat_tostr(test_path_prefix_, "post_simfile.out")),
			          post_simfile_.dump()) << "^ test " << test_case_name_;
		}

		void check_result_with_conver_log_out() const {
			EXPECT_EQ(getFileContents(
			             concat_tostr(test_path_prefix_, "conver_log.out")),
			          report_) << "^ test " << test_case_name_;
		}

		void check_result_with_judge_log_out() const {
			EXPECT_EQ(getFileContents(
			             concat_tostr(test_path_prefix_, "judge_log.out")),
			          serialized_judge_log()) << "^ test " << test_case_name_;
		}
	};
};

TEST(Conver, constructSimfile) {
	stdlog.label(false);
	ConverTestRunner(concat_tostr(getExecDir(getpid()), "conver_test_cases/"))
	   .run();
}
