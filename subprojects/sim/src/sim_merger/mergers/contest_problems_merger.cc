#include "..//progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_problems_merger.hh"
#include "contest_rounds_merger.hh"
#include "contests_merger.hh"
#include "modify_main_table_ids.hh"
#include "problems_merger.hh"
#include "upadte_jobs_using_table_ids.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contest_problems::ContestProblem;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
ContestProblemsIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(Select("created_at")
                                  .from("jobs")
                                  .where(
                                      "aux_id=? AND type IN (?, ?)",
                                      id,
                                      Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM,
                                      Job::Type::DELETE_CONTEST_PROBLEM
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

ContestProblemsMerger::ContestProblemsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const ContestsMerger& contests_merger,
    const ContestRoundsMerger& contest_rounds_merger,
    const ProblemsMerger& problems_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, contests_merger{contests_merger}
, contest_rounds_merger{contest_rounds_merger}
, problems_merger{problems_merger}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = ContestProblemsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void ContestProblemsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "contest_problems",
        [this](decltype(ContestProblem::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "contest_problems",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(ContestProblem::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(ContestProblem::id) main_id, decltype(ContestProblem::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(Update("jobs")
                                       .set("aux_id=?", new_id)
                                       .where(
                                           "type IN (?, ?) AND aux_id=?",
                                           Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM,
                                           Job::Type::DELETE_CONTEST_PROBLEM,
                                           main_id
                                       ));
        }
    );

    // Save other contest_problems
    auto other_contest_problems_copying_progress_printer =
        ProgressPrinter<decltype(ContestProblem::id)>{
            "progress: copying other contest_problem with id:"
        };
    static_assert(ContestProblem::COLUMNS_NUM == 9, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, created_at, contest_round_id, contest_id, problem_id, name, item, "
               "method_of_choosing_final_submission, score_revealing")
            .from("contest_problems")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    ContestProblem contest_problem;
    stmt.res_bind(
        contest_problem.id,
        contest_problem.created_at,
        contest_problem.contest_round_id,
        contest_problem.contest_id,
        contest_problem.problem_id,
        contest_problem.name,
        contest_problem.item,
        contest_problem.method_of_choosing_final_submission,
        contest_problem.score_revealing
    );
    while (stmt.next()) {
        other_contest_problems_copying_progress_printer.note_id(contest_problem.id);
        main_sim.mysql.execute(
            InsertInto(
                "contest_problems (id, created_at, contest_round_id, contest_id, problem_id, name, "
                "item, method_of_choosing_final_submission, score_revealing)"
            )
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?",
                    other_id_to_new_id(contest_problem.id),
                    contest_problem.created_at,
                    contest_rounds_merger.other_id_to_new_id(contest_problem.contest_round_id),
                    contests_merger.other_id_to_new_id(contest_problem.contest_id),
                    problems_merger.other_id_to_new_id(contest_problem.problem_id),
                    contest_problem.name,
                    contest_problem.item,
                    contest_problem.method_of_choosing_final_submission,
                    contest_problem.score_revealing
                )
        );
    }
}

} // namespace mergers
