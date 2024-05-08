#include "add_problem.hh"

#include <chrono>
#include <exception>
#include <sim/add_problem_jobs/add_problem_job.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/judging_config.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_perms.hh>
#include <simlib/file_remover.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/time.hh>
#include <zip.h>

using sim::add_problem_jobs::AddProblemJob;
using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void AddProblem::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    AddProblemJob add_problem_job;
    decltype(Job::creator) job_creator;
    auto stmt = mysql.execute(
        Select("apj.file_id, apj.visibility, apj.force_time_limits_reset, apj.ignore_simfile, "
               "apj.name, apj.label, apj.memory_limit_in_mib, apj.fixed_time_limit_in_ns, "
               "apj.reset_scoring, apj.look_for_new_tests, j.creator")
            .from("add_problem_jobs apj")
            .inner_join("jobs j")
            .on("j.id=apj.id")
            .where("apj.id=?", job_id_)
    );
    stmt.res_bind(
        add_problem_job.file_id,
        add_problem_job.visibility,
        add_problem_job.force_time_limits_reset,
        add_problem_job.ignore_simfile,
        add_problem_job.name,
        add_problem_job.label,
        add_problem_job.memory_limit_in_mib,
        add_problem_job.fixed_time_limit_in_ns,
        add_problem_job.reset_scoring,
        add_problem_job.look_for_new_tests,
        job_creator
    );
    throw_assert(stmt.next());

    /* Construct Simfile */

    sim::Conver conver;
    auto input_package_path = sim::internal_files::path_of(add_problem_job.file_id);
    conver.package_path(input_package_path);

    sim::Conver::Options options = {
        .name = (add_problem_job.name.empty() ? std::nullopt : std::optional{add_problem_job.name}),
        .label =
            (add_problem_job.label.empty() ? std::nullopt : std::optional{add_problem_job.label}),
        .interactive = std::nullopt,
        .memory_limit = add_problem_job.memory_limit_in_mib,
        .global_time_limit = add_problem_job.fixed_time_limit_in_ns
            ? std::optional{std::chrono::nanoseconds{*add_problem_job.fixed_time_limit_in_ns}}
            : std::nullopt,
        .max_time_limit = sim::MAX_TIME_LIMIT,
        .reset_time_limits_using_main_solution = add_problem_job.force_time_limits_reset,
        .ignore_simfile = add_problem_job.ignore_simfile,
        .seek_for_new_tests = add_problem_job.look_for_new_tests,
        .reset_scoring = add_problem_job.reset_scoring,
        .require_statement = true,
        .rtl_opts =
            {
                .min_time_limit = sim::MIN_TIME_LIMIT,
                .solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT,
            },
    };

    sim::Conver::ConstructionResult construction_res;
    try {
        construction_res = conver.construct_simfile(options);
    } catch (const std::exception& e) {
        return set_failure(conver.report(), "Conver failed: ", e.what());
    }

    const auto& problem_name = construction_res.simfile.name.value();
    if (problem_name.size() > decltype(Problem::name)::max_len) {
        return set_failure(
            "Problem name is too long (max allowed length: ", decltype(Problem::name)::max_len, ')'
        );
    }

    const auto& problem_label = construction_res.simfile.label.value();
    if (problem_label.size() > decltype(Problem::label)::max_len) {
        return set_failure(
            "Problem label is too long (max allowed length: ",
            decltype(Problem::label)::max_len,
            ')'
        );
    }

    switch (construction_res.status) {
    case sim::Conver::Status::COMPLETE: break;
    case sim::Conver::Status::NEED_MODEL_SOLUTION_JUDGE_REPORT: {
        job_log("Loading the problem package for judging the main solution...");
        sim::JudgeWorker judge_worker;
        judge_worker.load_package(input_package_path, construction_res.simfile.dump());
        const auto& main_solution_path = construction_res.simfile.solutions[0];
        job_log("Judging the model solution: ", main_solution_path);

        job_log("Compiling solution...");
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
            return set_failure("Compilation failed:\n", compilation_errors);
        }

        job_log("Compiling checker...");
        if (judge_worker.compile_checker(
                sim::CHECKER_COMPILATION_TIME_LIMIT,
                sim::CHECKER_COMPILATION_MEMORY_LIMIT,
                &compilation_errors,
                sim::COMPILATION_ERRORS_MAX_LENGTH
            ))
        {
            return set_failure("Compilation failed:\n", compilation_errors);
        }

        job_log("Judging...");
        auto logger = sim::VerboseJudgeLogger(true);
        sim::JudgeReport initial_judge_report = judge_worker.judge(false, logger);
        job_log_holder_.append("Initial judge report: ", initial_judge_report.judge_log);
        sim::JudgeReport final_judge_report = judge_worker.judge(true, logger);
        job_log_holder_.append("Final judge report: ", final_judge_report.judge_log);

        try {
            sim::Conver::ResetTimeLimitsOptions rtl_options;
            rtl_options.min_time_limit = sim::MIN_TIME_LIMIT;
            rtl_options.solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT;
            sim::Conver::reset_time_limits_using_jugde_reports(
                construction_res.simfile, initial_judge_report, final_judge_report, rtl_options
            );
        } catch (const std::exception& e) {
            return set_failure("Conver failed: ", e.what());
        }
    } break;
    }

    job_log("Creating package with a new Simfile...");
    auto curr_datetime = mysql_date();
    stmt = mysql.execute(InsertInto("internal_files (created_at)").values("?", curr_datetime));
    auto new_package_file_id = stmt.insert_id();
    auto new_package_path = sim::internal_files::path_of(new_package_file_id);
    auto new_package_file_remover =
        FileRemover{new_package_path}; // copy() may leave the dest file on error
    if (copy(input_package_path, new_package_path)) {
        return set_failure("copy()", errmsg());
    }
    auto zip = ZipFile{new_package_path};
    auto package_main_dir = sim::zip_package_main_dir(zip);
    auto new_simfile_contents = construction_res.simfile.dump();
    zip.file_add(
        concat(package_main_dir, "Simfile"),
        zip.source_buffer(new_simfile_contents),
        ZIP_FL_OVERWRITE
    );

    // Add problem to database
    auto problem_id = mysql
                          .execute(InsertInto("problems (created_at, file_id, type, name, label, "
                                              "simfile, owner_id, updated_at)")
                                       .values(
                                           "?, ?, ?, ?, ?, ?, ?, ?",
                                           curr_datetime,
                                           new_package_file_id,
                                           add_problem_job.visibility,
                                           problem_name,
                                           problem_label,
                                           construction_res.simfile.dump(),
                                           job_creator,
                                           curr_datetime
                                       ))
                          .insert_id();

    job_log("Submitting solutions...");
    std::vector<FileRemover> solution_file_removers;
    for (const auto& solution : construction_res.simfile.solutions) {
        job_log("Submitting: ", solution);
        auto solution_file_id =
            mysql.execute(InsertInto("internal_files (created_at)").values("?", curr_datetime))
                .insert_id();
        mysql.execute(
            InsertInto("submissions (created_at, file_id, owner, problem_id, contest_problem_id, "
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
        zip.extract_to_file(
            zip.get_index(concat(package_main_dir, solution)), solution_file_path, S_0600
        );
    }
    zip.close();

    // Add jobs to judge the solutions
    mysql.execute(InsertInto("jobs (created_at, file_id, tmp_file_id, creator, type, priority, "
                             "status, aux_id, info, data)")
                      .select(
                          "?, NULL, NULL, NULL, ?, ?, ?, id, '', ''",
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

    mysql.execute(sim::sql::Update("jobs").set("aux_id=?", problem_id).where("id=?", job_id_));
    job_done(mysql);

    transaction.commit();
    new_package_file_remover.cancel();
    for (auto& file_remover : solution_file_removers) {
        file_remover.cancel();
    }
}

} // namespace job_server::job_handlers
