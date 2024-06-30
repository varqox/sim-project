#include "add_or_reupload_problem/add_or_reupload_problem.hh"
#include "common.hh"
#include "judge_logger.hh"
#include "reset_problem_time_limits.hh"

#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/judging_config.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/sim/conver.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/time.hh>

using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace job_server::job_handlers {

void reset_problem_time_limits(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    auto transaction = mysql.start_repeatable_read_transaction();

    decltype(Problem::file_id) problem_file_id;
    {
        auto stmt = mysql.execute(Select("file_id").from("problems").where("id=?", problem_id));
        stmt.res_bind(problem_file_id);
        if (!stmt.next()) {
            logger("The problem with id: ", problem_id, " does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }
    }

    auto input_package_path = sim::internal_files::path_of(problem_file_id);
    sim::JudgeWorker judge_worker;
    logger("Loading problem package...");
    judge_worker.load_package(input_package_path, std::nullopt);
    logger("... done.");

    auto& simfile = judge_worker.simfile();
    simfile.load_all(); // Everything is needed as we will dump it to a new Simfile file.
    const auto& main_solution_path = simfile.solutions.at(0);
    logger("Model solution: ", main_solution_path);

    std::string compilation_errors;
    logger("Compiling the model solution...");
    if (judge_worker.compile_solution_from_package(
            main_solution_path,
            sim::filename_to_lang(main_solution_path),
            sim::SOLUTION_COMPILATION_TIME_LIMIT,
            sim::SOLUTION_COMPILATION_MEMORY_LIMIT,
            &compilation_errors,
            sim::COMPILATION_ERRORS_MAX_LENGTH
        ))
    {
        logger("... failed:\n", compilation_errors);
        mark_job_as_failed(mysql, logger, job_id);
        transaction.commit();
        return;
    }
    logger("... done.");

    compilation_errors = {};
    logger("Compiling checker...");
    if (judge_worker.compile_checker(
            sim::CHECKER_COMPILATION_TIME_LIMIT,
            sim::CHECKER_COMPILATION_MEMORY_LIMIT,
            &compilation_errors,
            sim::COMPILATION_ERRORS_MAX_LENGTH
        ))
    {
        logger("... failed:\n", compilation_errors);
        mark_job_as_failed(mysql, logger, job_id);
        transaction.commit();
        return;
    }
    logger("... done.");

    auto judge_logger = JudgeLogger{logger};
    auto initial_judge_report = judge_worker.judge(false, judge_logger);
    auto final_judge_report = judge_worker.judge(true, judge_logger);

    sim::Conver::ResetTimeLimitsOptions rtl_options;
    rtl_options.min_time_limit = sim::MIN_TIME_LIMIT;
    rtl_options.solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT;
    sim::Conver::reset_time_limits_using_jugde_reports(
        simfile, initial_judge_report, final_judge_report, rtl_options
    );

    auto current_datetime = utc_mysql_datetime();
    auto create_package_res = add_or_reupload_problem::create_package_with_simfile(
        mysql, logger, input_package_path, simfile, current_datetime
    );
    if (!create_package_res) {
        mark_job_as_failed(mysql, logger, job_id);
        transaction.commit();
        return;
    }
    create_package_res->new_package_zip.close();

    // Schedule job to delete the old problem file
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("problems")
                      .where("id=?", problem_id));

    // Update the problem
    mysql.execute(Update("problems")
                      .set(
                          "file_id=?, simfile=?, updated_at=?",
                          create_package_res->new_package_file_id,
                          simfile.dump(),
                          current_datetime
                      )
                      .where("id=?", problem_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
    create_package_res->new_package_file_remover.cancel();
}

} // namespace job_server::job_handlers
