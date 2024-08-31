#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "change_problem_statement_jobs_merger.hh"
#include "internal_files_merger.hh"
#include "jobs_merger.hh"

#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::change_problem_statement_jobs::ChangeProblemStatementJob;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

ChangeProblemStatementJobsMerger::ChangeProblemStatementJobsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const JobsMerger& jobs_merger,
    const InternalFilesMerger& internal_files_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, jobs_merger{jobs_merger}
, internal_files_merger{internal_files_merger} {}

void ChangeProblemStatementJobsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;
    // Main ids are already modified because JobsMerger changed jobs ids and foreign keys do the
    // work.

    // Save other change_problem_statement_jobs
    auto other_change_problem_statement_jobs_copying_progress_printer =
        ProgressPrinter<decltype(ChangeProblemStatementJob::id)>{
            "progress: copying other change_problem_statement_job with id:"
        };
    static_assert(ChangeProblemStatementJob::COLUMNS_NUM == 3, "Update the statements below");
    auto stmt = other_sim.mysql.execute(Select("id, new_statement_file_id, path_for_new_statement")
                                            .from("change_problem_statement_jobs")
                                            .order_by("id DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    ChangeProblemStatementJob change_problem_statement_job;
    stmt.res_bind(
        change_problem_statement_job.id,
        change_problem_statement_job.new_statement_file_id,
        change_problem_statement_job.path_for_new_statement
    );
    while (stmt.next()) {
        other_change_problem_statement_jobs_copying_progress_printer.note_id(
            change_problem_statement_job.id
        );
        main_sim.mysql.execute(
            InsertInto(
                "change_problem_statement_jobs (id, new_statement_file_id, path_for_new_statement)"
            )
                .values(
                    "?, ?, ?",
                    jobs_merger.other_id_to_new_id(change_problem_statement_job.id),
                    internal_files_merger.other_id_to_new_id(
                        change_problem_statement_job.new_statement_file_id
                    ),
                    change_problem_statement_job.path_for_new_statement
                )
        );
    }
}

} // namespace mergers
