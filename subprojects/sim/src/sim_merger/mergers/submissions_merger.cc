#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_problems_merger.hh"
#include "contest_rounds_merger.hh"
#include "contests_merger.hh"
#include "internal_files_merger.hh"
#include "modify_main_table_ids.hh"
#include "problems_merger.hh"
#include "submissions_merger.hh"
#include "upadte_jobs_using_table_ids.hh"
#include "users_merger.hh"

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::submissions::Submission;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
SubmissionsIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(Select("created_at")
                                  .from("jobs")
                                  .where(
                                      "aux_id=? AND type IN (?, ?)",
                                      id,
                                      Job::Type::JUDGE_SUBMISSION,
                                      Job::Type::REJUDGE_SUBMISSION
                                  )
                                  .order_by("id")
                                  .limit("1"));
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return created_at;
}

SubmissionsMerger::SubmissionsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const InternalFilesMerger& internal_files_merger,
    const UsersMerger& users_merger,
    const ProblemsMerger& problems_merger,
    const ContestProblemsMerger& contest_problems_merger,
    const ContestRoundsMerger& contest_rounds_merger,
    const ContestsMerger& contests_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, internal_files_merger{internal_files_merger}
, users_merger{users_merger}
, problems_merger{problems_merger}
, contest_problems_merger{contest_problems_merger}
, contest_rounds_merger{contest_rounds_merger}
, contests_merger{contests_merger}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = SubmissionsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void SubmissionsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "submissions",
        [this](decltype(Submission::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "submissions",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(Submission::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(Submission::id) main_id, decltype(Submission::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(Update("jobs")
                                       .set("aux_id=?", new_id)
                                       .where(
                                           "type IN (?, ?) AND aux_id=?",
                                           Job::Type::JUDGE_SUBMISSION,
                                           Job::Type::REJUDGE_SUBMISSION,
                                           main_id
                                       ));
        }
    );

    // Save other submissions
    auto other_submissions_copying_progress_printer =
        ProgressPrinter<decltype(Submission::id)>{"progress: copying other submission with id:"};
    static_assert(Submission::COLUMNS_NUM == 21, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select(
            "id, created_at, file_id, user_id, problem_id, contest_problem_id, contest_round_id, "
            "contest_id, type, language, initial_final_candidate, final_candidate, problem_final, "
            "contest_problem_final, contest_problem_initial_final, initial_status, full_status, "
            "score, last_judgment_began_at, initial_report, final_report"
        )
            .from("submissions")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    Submission submission;
    stmt.res_bind(
        submission.id,
        submission.created_at,
        submission.file_id,
        submission.user_id,
        submission.problem_id,
        submission.contest_problem_id,
        submission.contest_round_id,
        submission.contest_id,
        submission.type,
        submission.language,
        submission.initial_final_candidate,
        submission.final_candidate,
        submission.problem_final,
        submission.contest_problem_final,
        submission.contest_problem_initial_final,
        submission.initial_status,
        submission.full_status,
        submission.score,
        submission.last_judgment_began_at,
        submission.initial_report,
        submission.final_report
    );
    while (stmt.next()) {
        other_submissions_copying_progress_printer.note_id(submission.id);
        // Unset finals (they will be set by update_final() call below)
        submission.problem_final = false;
        submission.contest_problem_final = false;
        submission.contest_problem_initial_final = false;

        main_sim.mysql.execute(
            InsertInto("submissions (id, created_at, file_id, user_id, problem_id, "
                       "contest_problem_id, contest_round_id, contest_id, type, language, "
                       "initial_final_candidate, final_candidate, problem_final, "
                       "contest_problem_final, contest_problem_initial_final, initial_status, "
                       "full_status, score, last_judgment_began_at, initial_report, final_report)")
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?",
                    other_id_to_new_id(submission.id),
                    submission.created_at,
                    internal_files_merger.other_id_to_new_id(submission.file_id),
                    submission.user_id
                        ? std::optional{users_merger.other_id_to_new_id(*submission.user_id)}
                        : std::nullopt,
                    problems_merger.other_id_to_new_id(submission.problem_id),
                    submission.contest_problem_id
                        ? std::optional{contest_problems_merger.other_id_to_new_id(
                              *submission.contest_problem_id
                          )}
                        : std::nullopt,
                    submission.contest_round_id
                        ? std::optional{contest_rounds_merger.other_id_to_new_id(
                              *submission.contest_round_id
                          )}
                        : std::nullopt,
                    submission.contest_id
                        ? std::optional{contests_merger.other_id_to_new_id(*submission.contest_id)}
                        : std::nullopt,
                    submission.type,
                    submission.language,
                    submission.initial_final_candidate,
                    submission.final_candidate,
                    submission.problem_final,
                    submission.contest_problem_final,
                    submission.contest_problem_initial_final,
                    submission.initial_status,
                    submission.full_status,
                    submission.score,
                    submission.last_judgment_began_at,
                    submission.initial_report,
                    submission.final_report
                )
        );

        sim::submissions::update_final(
            main_sim.mysql, submission.user_id, submission.problem_id, submission.contest_problem_id
        );
    }
}

} // namespace mergers
