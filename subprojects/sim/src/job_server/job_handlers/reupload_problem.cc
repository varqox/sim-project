#include "add_or_reupload_problem/add_or_reupload_problem.hh"
#include "reupload_problem.hh"

#include <sim/jobs/job.hh>
#include <sim/reupload_problem_jobs/reupload_problem_job.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <simlib/file_remover.hh>
#include <simlib/throw_assert.hh>

using sim::jobs::Job;
using sim::reupload_problem_jobs::ReuploadProblemJob;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void ReuploadProblem::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    ReuploadProblemJob reupload_problem_job;

    auto stmt = mysql.execute(
        Select("file_id, problem_id, force_time_limits_reset, ignore_simfile, name, label, "
               "memory_limit_in_mib, fixed_time_limit_in_ns, reset_scoring, look_for_new_tests")
            .from("reupload_problem_jobs")
            .where("id=?", job_id_)
    );
    stmt.res_bind(
        reupload_problem_job.file_id,
        reupload_problem_job.problem_id,
        reupload_problem_job.force_time_limits_reset,
        reupload_problem_job.ignore_simfile,
        reupload_problem_job.name,
        reupload_problem_job.label,
        reupload_problem_job.memory_limit_in_mib,
        reupload_problem_job.fixed_time_limit_in_ns,
        reupload_problem_job.reset_scoring,
        reupload_problem_job.look_for_new_tests
    );
    throw_assert(stmt.next());

    auto input_package_path = sim::internal_files::path_of(reupload_problem_job.file_id);
    auto simfile = add_or_reupload_problem::construct_simfile(
        job_log_holder_,
        {
            .package_path = input_package_path,
            .name = reupload_problem_job.name,
            .label = reupload_problem_job.label,
            .memory_limit_in_mib = reupload_problem_job.memory_limit_in_mib,
            .fixed_time_limit_in_ns = reupload_problem_job.fixed_time_limit_in_ns,
            .force_time_limits_reset = reupload_problem_job.force_time_limits_reset,
            .ignore_simfile = reupload_problem_job.ignore_simfile,
            .look_for_new_tests = reupload_problem_job.look_for_new_tests,
            .reset_scoring = reupload_problem_job.reset_scoring,
        }
    );
    if (!simfile) {
        return set_failure();
    }

    auto curr_datetime = mysql_date();
    auto create_package_res = add_or_reupload_problem::create_package_with_simfile(
        job_log_holder_, mysql, input_package_path, *simfile, curr_datetime
    );
    if (!create_package_res) {
        return set_failure();
    }

    // Schedule job to delete the old problem file
    mysql.execute(InsertInto("jobs (created_at, file_id, creator, type, priority, "
                             "status, aux_id, info, data)")
                      .select(
                          "?, file_id, NULL, ?, ?, ?, NULL, '', ''",
                          curr_datetime,
                          Job::Type::DELETE_FILE,
                          default_priority(Job::Type::DELETE_FILE),
                          Job::Status::PENDING
                      )
                      .from("problems")
                      .where("id=?", reupload_problem_job.problem_id));
    // Update problem
    mysql.execute(Update("problems")
                      .set(
                          "file_id=?, name=?, label=?, simfile=?, updated_at=?",
                          create_package_res->new_package_file_id,
                          simfile->name.value(),
                          simfile->label.value(),
                          simfile->dump(),
                          curr_datetime
                      )
                      .where("id=?", reupload_problem_job.problem_id));

    // Schedule jobs to delete old solutions internal files
    mysql.execute(InsertInto("jobs (created_at, file_id, creator, type, priority, "
                             "status, aux_id, info, data)")
                      .select(
                          "?, file_id, NULL, ?, ?, ?, NULL, '', ''",
                          curr_datetime,
                          Job::Type::DELETE_FILE,
                          default_priority(Job::Type::DELETE_FILE),
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where(
                          "problem_id=? AND type=?",
                          reupload_problem_job.problem_id,
                          Submission::Type::PROBLEM_SOLUTION
                      ));
    // Delete old solutions' submissions
    mysql.execute(DeleteFrom("submissions")
                      .where(
                          "problem_id=? AND type=?",
                          reupload_problem_job.problem_id,
                          Submission::Type::PROBLEM_SOLUTION
                      ));

    // Submit new solutions
    auto solution_file_removers = add_or_reupload_problem::submit_solutions(
        job_log_holder_,
        mysql,
        *simfile,
        create_package_res->new_package_zip,
        create_package_res->new_package_main_dir,
        reupload_problem_job.problem_id,
        curr_datetime
    );

    // Commit changes to the new package zip
    create_package_res->new_package_zip.close();

    job_done(mysql);

    transaction.commit();
    create_package_res->new_package_file_remover.cancel();
    for (auto& file_remover : solution_file_removers) {
        file_remover.cancel();
    }
}

} // namespace job_server::job_handlers
