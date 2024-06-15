#include "add_or_reupload_problem.hh"

#include <chrono>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/judging_config.hh>
#include <sim/problems/problem.hh>
#include <sim/submissions/submission.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/logger.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/problem_package.hh>
#include <utility>

using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::submissions::Submission;

namespace job_server::job_handlers::add_or_reupload_problem {

std::optional<sim::Simfile>
construct_simfile(InplaceBuff<1 << 14>& job_log_holder, ConstructSimfileOptions&& options) {
    auto log = [&job_log_holder](auto&&... args) {
        return DoubleAppender<InplaceBuff<1 << 14>>(
            stdlog, job_log_holder, std::forward<decltype(args)>(args)...
        );
    };

    sim::Conver conver;
    conver.package_path(options.package_path);

    sim::Conver::Options conver_options = {
        .name = (options.name.empty() ? std::nullopt : std::optional{std::move(options).name}),
        .label = (options.label.empty() ? std::nullopt : std::optional{std::move(options).label}),
        .interactive = std::nullopt,
        .memory_limit = std::move(options).memory_limit_in_mib,
        .global_time_limit = options.fixed_time_limit_in_ns
            ? std::optional{std::chrono::nanoseconds{*std::move(options).fixed_time_limit_in_ns}}
            : std::nullopt,
        .max_time_limit = sim::MAX_TIME_LIMIT,
        .reset_time_limits_using_main_solution = std::move(options).force_time_limits_reset,
        .ignore_simfile = std::move(options).ignore_simfile,
        .seek_for_new_tests = std::move(options).look_for_new_tests,
        .reset_scoring = std::move(options).reset_scoring,
        .require_statement = true,
        .rtl_opts =
            {
                .min_time_limit = sim::MIN_TIME_LIMIT,
                .solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT,
            },
    };

    sim::Conver::ConstructionResult construction_res;
    try {
        construction_res = conver.construct_simfile(conver_options);
    } catch (const std::exception& e) {
        log(conver.report(), "Conver failed: ", e.what());
        return std::nullopt;
    }

    if (construction_res.simfile.name.value().size() > decltype(Problem::name)::max_len) {
        log("Problem name is too long (max allowed length: ", decltype(Problem::name)::max_len, ')'
        );
        return std::nullopt;
    }

    if (construction_res.simfile.label.value().size() > decltype(Problem::label)::max_len) {
        log("Problem label is too long (max allowed length: ",
            decltype(Problem::label)::max_len,
            ')');
        return std::nullopt;
    }

    switch (construction_res.status) {
    case sim::Conver::Status::COMPLETE: break;
    case sim::Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
        log("Loading the problem package for judging the main solution...");
        sim::JudgeWorker judge_worker;
        judge_worker.load_package(std::move(options).package_path, construction_res.simfile.dump());
        const auto& main_solution_path = construction_res.simfile.solutions[0];
        log("Judging the model solution: ", main_solution_path);

        log("Compiling solution...");
        std::string compilation_errors;
        if (judge_worker.compile_solution_from_package(
                main_solution_path,
                sim::filename_to_lang(main_solution_path),
                sim::SOLUTION_COMPILATION_TIME_LIMIT,
                sim::SOLUTION_COMPILATION_MEMORY_LIMIT,
                &compilation_errors,
                sim::COMPILATION_ERRORS_MAX_LENGTH
            ))
        {
            log("Compilation failed:\n", compilation_errors);
            return std::nullopt;
        }

        log("Compiling checker...");
        if (judge_worker.compile_checker(
                sim::CHECKER_COMPILATION_TIME_LIMIT,
                sim::CHECKER_COMPILATION_MEMORY_LIMIT,
                &compilation_errors,
                sim::COMPILATION_ERRORS_MAX_LENGTH
            ))
        {
            log("Compilation failed:\n", compilation_errors);
            return std::nullopt;
        }

        log("Judging...");
        auto logger = sim::VerboseJudgeLogger(true);
        sim::JudgeReport initial_judge_report = judge_worker.judge(false, logger);
        job_log_holder.append("Initial judge report: ", initial_judge_report.judge_log);
        sim::JudgeReport final_judge_report = judge_worker.judge(true, logger);
        job_log_holder.append("Final judge report: ", final_judge_report.judge_log);

        try {
            sim::Conver::ResetTimeLimitsOptions rtl_options;
            rtl_options.min_time_limit = sim::MIN_TIME_LIMIT;
            rtl_options.solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT;
            sim::Conver::reset_time_limits_using_jugde_reports(
                construction_res.simfile, initial_judge_report, final_judge_report, rtl_options
            );
        } catch (const std::exception& e) {
            log("Conver failed: ", e.what());
            return std::nullopt;
        }
    } break;
    }

    return std::move(construction_res.simfile);
}

std::optional<CreatePackageWithSimfileRes> create_package_with_simfile(
    InplaceBuff<1 << 14>& job_log_holder,
    sim::mysql::Connection& mysql,
    const std::string& input_package_path,
    const sim::Simfile& simfile,
    const std::string& curr_datetime
) {
    auto log = [&job_log_holder](auto&&... args) {
        return DoubleAppender<InplaceBuff<1 << 14>>(
            stdlog, job_log_holder, std::forward<decltype(args)>(args)...
        );
    };

    log("Creating package with a new Simfile...");
    auto new_package_file_id = sim::internal_files::new_internal_file_id(mysql, curr_datetime);
    auto new_package_path = sim::internal_files::path_of(new_package_file_id);
    auto new_package_file_remover =
        FileRemover{new_package_path}; // copy() may leave the dest file on error
    if (copy(input_package_path, new_package_path)) {
        log("copy()", errmsg());
        return std::nullopt;
    }
    auto zip = ZipFile{new_package_path};
    auto package_main_dir = sim::zip_package_main_dir(zip);
    auto new_simfile_contents = simfile.dump();
    zip.file_add(
        concat(package_main_dir, "Simfile"),
        zip.source_buffer(new_simfile_contents),
        ZIP_FL_OVERWRITE
    );

    return CreatePackageWithSimfileRes{
        .new_package_file_id = new_package_file_id,
        .new_package_main_dir = std::move(package_main_dir),
        .new_package_zip = std::move(zip),
        .new_package_file_remover = std::move(new_package_file_remover),
    };
}

std::vector<FileRemover> submit_solutions(
    InplaceBuff<1 << 14>& job_log_holder,
    sim::mysql::Connection& mysql,
    const sim::Simfile& simfile,
    ZipFile& package_zip,
    const std::string& package_main_dir,
    decltype(sim::problems::Problem::id) problem_id,
    const std::string& curr_datetime
) {

    auto log = [&job_log_holder](auto&&... args) {
        return DoubleAppender<InplaceBuff<1 << 14>>(
            stdlog, job_log_holder, std::forward<decltype(args)>(args)...
        );
    };

    log("Submitting solutions...");
    std::vector<FileRemover> solution_file_removers;
    for (const auto& solution : simfile.solutions) {
        log("Submitting: ", solution);
        auto solution_file_id = sim::internal_files::new_internal_file_id(mysql, curr_datetime);
        mysql.execute(
            InsertInto("submissions (created_at, file_id, user_id, problem_id, contest_problem_id, "
                       "contest_round_id, contest_id, type, language, initial_status, full_status, "
                       "last_judgment, initial_report, final_report)")
                .values(
                    "?, ?, NULL, ?, NULL, NULL, NULL, ?, ?, ?, ?, ?, '', ''",
                    curr_datetime,
                    solution_file_id,
                    problem_id,
                    Submission::Type::PROBLEM_SOLUTION,
                    sim::filename_to_lang(solution),
                    Submission::Status::PENDING,
                    Submission::Status::PENDING,
                    curr_datetime // TODO: NULL here and initial_report and final_report
                )
        );
        // Save the submission source code
        auto solution_file_path = sim::internal_files::path_of(solution_file_id);
        solution_file_removers.emplace_back(solution_file_path);
        package_zip.extract_to_file(
            package_zip.get_index(concat(package_main_dir, solution)), solution_file_path, S_0600
        );
    }

    // Add jobs to judge the solutions
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, id",
                          curr_datetime,
                          Job::Type::JUDGE_SUBMISSION,
                          default_priority(Job::Type::JUDGE_SUBMISSION) + 1,
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where(
                          "problem_id=? AND type=? ORDER BY id",
                          problem_id,
                          Submission::Type::PROBLEM_SOLUTION
                      ));

    return solution_file_removers;
}

} // namespace job_server::job_handlers::add_or_reupload_problem
