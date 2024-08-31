#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "add_problem_jobs_merger.hh"
#include "internal_files_merger.hh"
#include "jobs_merger.hh"

#include <sim/add_problem_jobs/add_problem_job.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::add_problem_jobs::AddProblemJob;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

AddProblemJobsMerger::AddProblemJobsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const JobsMerger& jobs_merger,
    const InternalFilesMerger& internal_files_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, jobs_merger{jobs_merger}
, internal_files_merger{internal_files_merger} {}

void AddProblemJobsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;
    // Main ids are already modified because JobsMerger changed jobs ids and foreign keys do the
    // work.

    // Save other add_problem_jobs
    auto other_add_problem_jobs_copying_progress_printer =
        ProgressPrinter<decltype(AddProblemJob::id)>{
            "progress: copying other add_problem_job with id:"
        };
    static_assert(AddProblemJob::COLUMNS_NUM == 11, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, file_id, visibility, force_time_limits_reset, ignore_simfile, name, label, "
               "memory_limit_in_mib, fixed_time_limit_in_ns, reset_scoring, look_for_new_tests")
            .from("add_problem_jobs")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    AddProblemJob add_problem_job;
    stmt.res_bind(
        add_problem_job.id,
        add_problem_job.file_id,
        add_problem_job.visibility,
        add_problem_job.force_time_limits_reset,
        add_problem_job.ignore_simfile,
        add_problem_job.name,
        add_problem_job.label,
        add_problem_job.memory_limit_in_mib,
        add_problem_job.fixed_time_limit_in_ns,
        add_problem_job.reset_scoring,
        add_problem_job.look_for_new_tests
    );
    while (stmt.next()) {
        other_add_problem_jobs_copying_progress_printer.note_id(add_problem_job.id);
        main_sim.mysql.execute(
            InsertInto("add_problem_jobs (id, file_id, visibility, force_time_limits_reset, "
                       "ignore_simfile, name, label, memory_limit_in_mib, fixed_time_limit_in_ns, "
                       "reset_scoring, look_for_new_tests)")
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?",
                    jobs_merger.other_id_to_new_id(add_problem_job.id),
                    internal_files_merger.other_id_to_new_id(add_problem_job.file_id),
                    add_problem_job.visibility,
                    add_problem_job.force_time_limits_reset,
                    add_problem_job.ignore_simfile,
                    add_problem_job.name,
                    add_problem_job.label,
                    add_problem_job.memory_limit_in_mib,
                    add_problem_job.fixed_time_limit_in_ns,
                    add_problem_job.reset_scoring,
                    add_problem_job.look_for_new_tests
                )
        );
    }
}

} // namespace mergers
