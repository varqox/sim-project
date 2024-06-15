#include "add_or_reupload_problem/add_or_reupload_problem.hh"
#include "add_problem.hh"

#include <sim/add_problem_jobs/add_problem_job.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/file_remover.hh>
#include <simlib/libzip.hh>
#include <simlib/time.hh>

using sim::add_problem_jobs::AddProblemJob;
using sim::jobs::Job;
using sim::sql::InsertInto;
using sim::sql::Select;

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

    auto input_package_path = sim::internal_files::path_of(add_problem_job.file_id);
    auto simfile = add_or_reupload_problem::construct_simfile(
        job_log_holder_,
        {
            .package_path = input_package_path,
            .name = add_problem_job.name,
            .label = add_problem_job.label,
            .memory_limit_in_mib = add_problem_job.memory_limit_in_mib,
            .fixed_time_limit_in_ns = add_problem_job.fixed_time_limit_in_ns,
            .force_time_limits_reset = add_problem_job.force_time_limits_reset,
            .ignore_simfile = add_problem_job.ignore_simfile,
            .look_for_new_tests = add_problem_job.look_for_new_tests,
            .reset_scoring = add_problem_job.reset_scoring,
        }
    );
    if (!simfile) {
        return set_failure();
    }

    auto curr_datetime = utc_mysql_datetime();
    auto create_package_res = add_or_reupload_problem::create_package_with_simfile(
        job_log_holder_, mysql, input_package_path, *simfile, curr_datetime
    );
    if (!create_package_res) {
        return set_failure();
    }

    // Add problem to database
    auto problem_id =
        mysql
            .execute(InsertInto("problems (created_at, file_id, visibility, name, label, "
                                "simfile, owner_id, updated_at)")
                         .values(
                             "?, ?, ?, ?, ?, ?, ?, ?",
                             curr_datetime,
                             create_package_res->new_package_file_id,
                             add_problem_job.visibility,
                             simfile->name.value(),
                             simfile->label.value(),
                             simfile->dump(),
                             job_creator,
                             curr_datetime
                         ))
            .insert_id();

    auto solution_file_removers = add_or_reupload_problem::submit_solutions(
        job_log_holder_,
        mysql,
        *simfile,
        create_package_res->new_package_zip,
        create_package_res->new_package_main_dir,
        problem_id,
        curr_datetime
    );

    // Commit changes to the new package zip
    create_package_res->new_package_zip.close();

    mysql.execute(sim::sql::Update("jobs").set("aux_id=?", problem_id).where("id=?", job_id_));
    mysql.execute(sim::sql::Update("add_problem_jobs")
                      .set("added_problem_id=?", problem_id)
                      .where("id=?", job_id_));

    job_done(mysql);

    transaction.commit();
    create_package_res->new_package_file_remover.cancel();
    for (auto& file_remover : solution_file_removers) {
        file_remover.cancel();
    }
}

} // namespace job_server::job_handlers
