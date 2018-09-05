#include "../include/process.h"
#include "../include/sim/conver.h"

#include <gtest/gtest.h>

using sim::Conver;
using sim::JudgeReport;
using sim::JudgeWorker;
using std::string;

static Conver::Options load_options_from_file(CStringView file) {
	ConfigFile cf;
	cf.loadConfigFromFile(file, true);

	auto get_var = [&](StringView name) -> decltype(auto) {
		auto const& var = cf[name];
		if (not var.isSet())
			THROW("Variable \"", name, "\" is not set");
		if (var.isArray())
			THROW("Variable \"", name, "\" is an array");
		return var;
	};

	auto get_string = [&](StringView name) -> decltype(auto) {
		return get_var(name).asString();
	};

	auto get_uint64 = [&](StringView name) {
		return get_var(name).asInt<uint64_t>();
	};

	auto get_bool = [&](StringView name) {
		StringView str = get_string(name);
		if (str == "true")
			return true;
		if (str == "false")
			return false;
		THROW("variable \"", name, "\" is not a bool: ", str);
	};

	Conver::Options opts;

	opts.name = get_string("name");
	opts.label = get_string("label");
	opts.memory_limit = get_uint64("memory_limit");
	opts.global_time_limit = get_uint64("global_time_limit");
	opts.reset_time_limits_using_model_solution =
		get_bool("reset_time_limits_using_model_solution");
	opts.ignore_simfile = get_bool("ignore_simfile");
	opts.seek_for_new_tests = get_bool("seek_for_new_tests");
	opts.reset_scoring = get_bool("reset_scoring");

	return opts;
}


TEST (Conver, constructSimfile) {
	stdlog.label(false);
	TemporaryFile package_copy("/tmp/conver_test.XXXXXX");

	constexpr bool regenerate_outs = false;

	auto exec_dir = getExecDir(getpid());
	AVLDictSet<string, StrNumCompare> cases;
	// Detect available test cases
	forEachDirComponent(concat(exec_dir, "conver_test_cases/").to_cstr(),
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
			auto options = load_options_from_file(concat_tostr(base, "conver.options"));

			copy(concat_tostr(base, "package.zip"), package_copy.path());

			Conver conver;
			conver.setPackagePath(package_copy.path());
			auto cres = conver.constructSimfile(options);

			auto check_result = [&] {
				// Lower time limits lower than 1 s to 0. This should remove the
				// problem with random time limit if they were set using the model
				// solution.
				for (auto& group : cres.simfile.tgroups)
					for (auto& test : group.tests) {
						EXPECT_GT(test.time_limit, 0); // Time limits should not have been set to 0
						if (test.time_limit < 1e6)
							test.time_limit = 0;
					}

				if (regenerate_outs) {
					putFileContents(concat_tostr(base, "simfile.out"),
						cres.simfile.dump());
					putFileContents(concat_tostr(base, "conver_log.out"),
						conver.getReport());
				}

				EXPECT_EQ(getFileContents(concat_tostr(base, "simfile.out")),
					cres.simfile.dump());

				EXPECT_EQ(getFileContents(concat_tostr(base, "conver_log.out")),
					conver.getReport());
			};

			switch (cres.status) {
			case Conver::Status::COMPLETE: check_result(); break;
			case Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
				JudgeWorker jworker;
				jworker.loadPackage(package_copy.path(), cres.simfile.dump());

				string compilation_errors;
				if (jworker.compileChecker({5, 0}, &compilation_errors, 4096, ""))
					THROW("failed to compile checker: \n", compilation_errors);

				TemporaryFile sol_src("/tmp/problem_solution.XXXXXX");
				writeAll(sol_src, extract_file_from_zip(package_copy.path(),
					concat(cres.pkg_master_dir, cres.simfile.solutions[0])));

				if (jworker.compileSolution(sol_src.path(),
					sim::filename_to_lang(cres.simfile.solutions[0]), {5, 0},
					&compilation_errors, 4096, ""))
				{
					THROW("failed to compile solution: \n", compilation_errors);
				}

				JudgeReport rep1 = jworker.judge(false);
				JudgeReport rep2 = jworker.judge(true);

				conver.finishConstructingSimfile(cres.simfile, rep1, rep2);

				check_result();
				break;
			}
			}
		} catch (const std::exception& e) {
			FAIL() << "Unexpected exception -> " << e.what();
		}
	});
}
