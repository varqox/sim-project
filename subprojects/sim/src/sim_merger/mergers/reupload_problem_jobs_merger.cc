#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "internal_files_merger.hh"
#include "jobs_merger.hh"
#include "reupload_problem_jobs_merger.hh"

#include <sim/reupload_problem_jobs/reupload_problem_job.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::reupload_problem_jobs::ReuploadProblemJob;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

ReuploadProblemJobsMerger::ReuploadProblemJobsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const JobsMerger& jobs_merger,
    const InternalFilesMerger& internal_files_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, jobs_merger{jobs_merger}
, internal_files_merger{internal_files_merger} {}

void ReuploadProblemJobsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;
    // Main ids are already modified because JobsMerger changed jobs ids and foreign keys do the
    // work.

    // Save other reupload_problem_jobs
    auto other_reupload_problem_jobs_copying_progress_printer =
        ProgressPrinter<decltype(ReuploadProblemJob::id)>{
            "progress: copying other reupload_problem_job with id:"
        };
    static_assert(ReuploadProblemJob::COLUMNS_NUM == 10, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, file_id, force_time_limits_reset, ignore_simfile, name, label, "
               "memory_limit_in_mib, fixed_time_limit_in_ns, reset_scoring, look_for_new_tests")
            .from("reupload_problem_jobs")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    ReuploadProblemJob reupload_problem_job;
    stmt.res_bind(
        reupload_problem_job.id,
        reupload_problem_job.file_id,
        reupload_problem_job.force_time_limits_reset,
        reupload_problem_job.ignore_simfile,
        reupload_problem_job.name,
        reupload_problem_job.label,
        reupload_problem_job.memory_limit_in_mib,
        reupload_problem_job.fixed_time_limit_in_ns,
        reupload_problem_job.reset_scoring,
        reupload_problem_job.look_for_new_tests
    );
    while (stmt.next()) {
        other_reupload_problem_jobs_copying_progress_printer.note_id(reupload_problem_job.id);
        main_sim.mysql.execute(
            InsertInto("reupload_problem_jobs (id, file_id, force_time_limits_reset, "
                       "ignore_simfile, name, label, memory_limit_in_mib, fixed_time_limit_in_ns, "
                       "reset_scoring, look_for_new_tests)")
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?",
                    jobs_merger.other_id_to_new_id(reupload_problem_job.id),
                    internal_files_merger.other_id_to_new_id(reupload_problem_job.file_id),
                    reupload_problem_job.force_time_limits_reset,
                    reupload_problem_job.ignore_simfile,
                    reupload_problem_job.name,
                    reupload_problem_job.label,
                    reupload_problem_job.memory_limit_in_mib,
                    reupload_problem_job.fixed_time_limit_in_ns,
                    reupload_problem_job.reset_scoring,
                    reupload_problem_job.look_for_new_tests
                )
        );
    }
}

} // namespace mergers
