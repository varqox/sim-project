#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "jobs_merger.hh"
#include "merge_problems_jobs_merger.hh"

#include <sim/merge_problems_jobs/merge_problems_job.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::merge_problems_jobs::MergeProblemsJob;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

MergeProblemsJobsMerger::MergeProblemsJobsMerger(
    const SimInstance& main_sim, const SimInstance& other_sim, const JobsMerger& jobs_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, jobs_merger{jobs_merger} {}

void MergeProblemsJobsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;
    // Main ids are already modified because JobsMerger changed jobs ids and foreign keys do the
    // work.

    // Save other merge_problems_jobs
    auto other_merge_problems_jobs_copying_progress_printer =
        ProgressPrinter<decltype(MergeProblemsJob::id)>{
            "progress: copying other merge_problems_job with id:"
        };
    static_assert(MergeProblemsJob::COLUMNS_NUM == 2, "Update the statements below");
    auto stmt = other_sim.mysql.execute(Select("id, rejudge_transferred_submissions")
                                            .from("merge_problems_jobs")
                                            .order_by("id DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    MergeProblemsJob merge_problems_job;
    stmt.res_bind(merge_problems_job.id, merge_problems_job.rejudge_transferred_submissions);
    while (stmt.next()) {
        other_merge_problems_jobs_copying_progress_printer.note_id(merge_problems_job.id);
        main_sim.mysql.execute(
            InsertInto("merge_problems_jobs (id, rejudge_transferred_submissions)")
                .values(
                    "?, ?",
                    jobs_merger.other_id_to_new_id(merge_problems_job.id),
                    merge_problems_job.rejudge_transferred_submissions
                )
        );
    }
}

} // namespace mergers
