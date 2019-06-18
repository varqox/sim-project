#include "../include/sim/conver.h"
#include "../include/avl_dict.h"
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

	auto get_optional_uint64 = [&](StringView name) -> Optional<uint64_t> {
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
	   [&](StringView name) -> Optional<std::chrono::nanoseconds> {
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

	auto get_optional_bool = [&](StringView name) -> Optional<bool> {
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

TEST(Conver, constructSimfile) {
	// TODO: make tests for interactive problem packages
	stdlog.label(false);
	TemporaryFile package_copy("/tmp/conver_test.XXXXXX");

	constexpr bool regenerate_outs = false;

	using std::chrono_literals::operator""s;
	using std::chrono::duration_cast;
	using std::chrono::seconds;

	auto exec_dir = getExecDir(getpid());
	AVLDictSet<string, StrNumCompare> cases;
	// Detect available test cases
	forEachDirComponent(concat(exec_dir, "conver_test_cases/"),
	                    [&](dirent* file) {
		                    constexpr StringView suff("package.zip");
		                    StringView file_name(file->d_name);
		                    if (hasSuffix(file_name, suff)) {
			                    file_name.removeSuffix(suff.size());
			                    cases.emplace(file_name.to_string());
		                    }
	                    });

	cases.for_each([&](StringView i) {
		stdlog("Test case: ", i);
		try {
			auto base = concat(exec_dir, "conver_test_cases/", i);
			auto options =
			   load_options_from_file(concat_tostr(base, "conver.options"));

			copy(concat_tostr(base, "package.zip"), package_copy.path());

			Conver conver;
			conver.package_path(package_copy.path());
			std::string report;
			sim::Simfile pre_simfile, post_simfile;

			auto check_result = [&] {
				// Round time limits to whole seconds. This should remove the
				// problem with random time limit if they were set using the
				// model solution.
				for (auto& group : post_simfile.tgroups) {
					for (auto& test : group.tests) {
						EXPECT_GT(
						   test.time_limit,
						   0s); // Time limits should not have been set to 0
						test.time_limit =
						   duration_cast<seconds>(test.time_limit + 0.5s);
					}
				}

				if (regenerate_outs) {
					putFileContents(
					   concat_tostr(base, "pre_simfile.out"),
					   intentionalUnsafeStringView(pre_simfile.dump()));
					putFileContents(
					   concat_tostr(base, "post_simfile.out"),
					   intentionalUnsafeStringView(post_simfile.dump()));
					putFileContents(concat_tostr(base, "conver_log.out"),
					                report);
				}

				EXPECT_EQ(
				   getFileContents(concat_tostr(base, "pre_simfile.out")),
				   pre_simfile.dump());

				EXPECT_EQ(
				   getFileContents(concat_tostr(base, "post_simfile.out")),
				   post_simfile.dump());

				EXPECT_EQ(getFileContents(concat_tostr(base, "conver_log.out")),
				          report);
			};

			try {
				auto cres = conver.construct_simfile(options);
				pre_simfile = cres.simfile;
				post_simfile = cres.simfile;
				report = conver.report();

				switch (cres.status) {
				case Conver::Status::COMPLETE: break;

				case Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
					JudgeWorker jworker;
					jworker.load_package(package_copy.path(),
					                     post_simfile.dump());

					string compilation_errors;
					if (jworker.compile_checker(5s, &compilation_errors, 4096,
					                            "")) {
						THROW("failed to compile checker: \n",
						      compilation_errors);
					}

					TemporaryFile sol_src("/tmp/problem_solution.XXXXXX");
					ZipFile zip(package_copy.path());
					zip.extract_to_fd(
					   zip.get_index(concat(cres.pkg_master_dir,
					                        post_simfile.solutions[0])),
					   sol_src);

					if (jworker.compile_solution(
					       sol_src.path(),
					       sim::filename_to_lang(post_simfile.solutions[0]), 5s,
					       &compilation_errors, 4096, "")) {
						THROW("failed to compile solution: \n",
						      compilation_errors);
					}

					JudgeReport rep1 = jworker.judge(false);
					JudgeReport rep2 = jworker.judge(true);

					Conver::reset_time_limits_using_jugde_reports(
					   post_simfile, rep1, rep2, options.rtl_opts);

					break;
				}
				}
			} catch (const std::exception& e) {
				report = conver.report();
				back_insert(report, "\n>>>> Exception caught <<<<\n", e.what());
			}

			check_result();

		} catch (const std::exception& e) {
			FAIL() << "Unexpected exception -> " << e.what();
		}
	});
}
