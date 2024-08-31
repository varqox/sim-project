#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_problems_merger.hh"
#include "contest_rounds_merger.hh"
#include "contests_merger.hh"
#include "internal_files_merger.hh"
#include "jobs_merger.hh"
#include "modify_main_table_ids.hh"
#include "problems_merger.hh"
#include "submissions_merger.hh"
#include "users_merger.hh"

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/macros/throw.hh>

using sim::jobs::Job;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
JobsIdIterator::created_at_upper_bound_of_id(uint64_t /*id*/) {
    return std::nullopt;
}

JobsMerger::JobsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const InternalFilesMerger& internal_files_merger,
    const UsersMerger& users_merger,
    const ProblemsMerger& problems_merger,
    const ContestsMerger& contests_merger,
    const ContestRoundsMerger& contest_rounds_merger,
    const ContestProblemsMerger& contest_problems_merger,
    const SubmissionsMerger& submissions_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, internal_files_merger{internal_files_merger}
, users_merger{users_merger}
, problems_merger{problems_merger}
, contests_merger{contests_merger}
, contest_rounds_merger{contest_rounds_merger}
, contest_problems_merger{contest_problems_merger}
, submissions_merger{submissions_merger}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = JobsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void JobsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(main_ids_iter, main_sim.mysql, "jobs", [this](decltype(Job::id) main_id) {
        return main_id_to_new_id(main_id);
    });

    // Save other jobs
    auto other_jobs_copying_progress_printer =
        ProgressPrinter<decltype(Job::id)>{"progress: copying other job with id:"};
    static_assert(Job::COLUMNS_NUM == 9, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, created_at, creator, type, priority, status, aux_id, aux_id_2, log")
            .from("jobs")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    Job job;
    stmt.res_bind(
        job.id,
        job.created_at,
        job.creator,
        job.type,
        job.priority,
        job.status,
        job.aux_id,
        job.aux_id_2,
        job.log
    );
    while (stmt.next()) {
        other_jobs_copying_progress_printer.note_id(job.id);
        main_sim.mysql.execute(
            InsertInto(
                "jobs (id, created_at, creator, type, priority, status, aux_id, aux_id_2, log)"
            )
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?",
                    other_id_to_new_id(job.id),
                    job.created_at,
                    job.creator ? std::optional{users_merger.other_id_to_new_id(*job.creator)}
                                : std::nullopt,
                    job.type,
                    job.priority,
                    job.status,
                    job.aux_id ? std::optional{[&] {
                        switch (job.type) {
                        case Job::Type::JUDGE_SUBMISSION:
                        case Job::Type::REJUDGE_SUBMISSION:
                            return submissions_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::ADD_PROBLEM:
                        case Job::Type::REUPLOAD_PROBLEM:
                        case Job::Type::EDIT_PROBLEM:
                        case Job::Type::DELETE_PROBLEM:
                        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
                        case Job::Type::MERGE_PROBLEMS:
                        case Job::Type::CHANGE_PROBLEM_STATEMENT:
                            return problems_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::DELETE_CONTEST_PROBLEM:
                        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                            return contest_problems_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::DELETE_USER:
                        case Job::Type::MERGE_USERS:
                            return users_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::DELETE_CONTEST:
                            return contests_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::DELETE_CONTEST_ROUND:
                            return contest_rounds_merger.other_id_to_new_id(*job.aux_id);
                        case Job::Type::DELETE_INTERNAL_FILE:
                            return internal_files_merger.other_id_to_new_id(*job.aux_id);
                        }
                        THROW("invalid job.type");
                    }()}
                               : std::nullopt,
                    job.aux_id_2 ? std::optional{[&] {
                        switch (job.type) {
                        case Job::Type::MERGE_PROBLEMS:
                            return problems_merger.other_id_to_new_id(*job.aux_id_2);
                        case Job::Type::MERGE_USERS:
                            return users_merger.other_id_to_new_id(*job.aux_id_2);
                        case Job::Type::JUDGE_SUBMISSION:
                        case Job::Type::REJUDGE_SUBMISSION:
                        case Job::Type::ADD_PROBLEM:
                        case Job::Type::REUPLOAD_PROBLEM:
                        case Job::Type::EDIT_PROBLEM:
                        case Job::Type::DELETE_PROBLEM:
                        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
                        case Job::Type::CHANGE_PROBLEM_STATEMENT:
                        case Job::Type::DELETE_CONTEST_PROBLEM:
                        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                        case Job::Type::DELETE_USER:
                        case Job::Type::DELETE_CONTEST:
                        case Job::Type::DELETE_CONTEST_ROUND:
                        case Job::Type::DELETE_INTERNAL_FILE: break;
                        }
                        THROW(
                            "non-null job.aux_id_2 for unexpected job.type = ", job.type.to_str()
                        );
                    }()}
                                 : std::nullopt,
                    job.log
                )
        );
    }
}

} // namespace mergers
