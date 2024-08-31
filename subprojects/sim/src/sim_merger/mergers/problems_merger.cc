#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "internal_files_merger.hh"
#include "modify_main_table_ids.hh"
#include "problems_merger.hh"
#include "upadte_jobs_using_table_ids.hh"
#include "users_merger.hh"

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
ProblemsIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(
        Select("created_at")
            .from("jobs")
            .where(
                "(aux_id=? AND type IN (?, ?, ?, ?, ?, ?, ?)) OR (aux_id_2=? AND type=?)",
                id,
                Job::Type::ADD_PROBLEM,
                Job::Type::REUPLOAD_PROBLEM,
                Job::Type::EDIT_PROBLEM,
                Job::Type::DELETE_PROBLEM,
                Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION,
                Job::Type::MERGE_PROBLEMS,
                Job::Type::CHANGE_PROBLEM_STATEMENT,
                id,
                Job::Type::MERGE_PROBLEMS
            )
            .order_by("id")
            .limit("1")
    );
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return created_at;
}

ProblemsMerger::ProblemsMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const InternalFilesMerger& internal_files_merger,
    const UsersMerger& users_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, internal_files_merger{internal_files_merger}
, users_merger{users_merger}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = ProblemsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void ProblemsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "problems",
        [this](decltype(Problem::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "problems",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(Problem::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(Problem::id) main_id, decltype(Problem::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id=?", new_id)
                    .where(
                        "type IN (?, ?, ?, ?, ?, ?, ?) AND aux_id=?",
                        Job::Type::ADD_PROBLEM,
                        Job::Type::REUPLOAD_PROBLEM,
                        Job::Type::EDIT_PROBLEM,
                        Job::Type::DELETE_PROBLEM,
                        Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION,
                        Job::Type::MERGE_PROBLEMS,
                        Job::Type::CHANGE_PROBLEM_STATEMENT,
                        main_id
                    )
            );
            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id_2=?", new_id)
                    .where("type=? AND aux_id_2=?", Job::Type::MERGE_PROBLEMS, main_id)
            );
        }
    );

    // Save other problems
    auto other_problems_copying_progress_printer =
        ProgressPrinter<decltype(Problem::id)>{"progress: copying other problem with id:"};
    static_assert(Problem::COLUMNS_NUM == 9, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, created_at, file_id, visibility, name, label, simfile, owner_id, updated_at")
            .from("problems")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    Problem problem;
    stmt.res_bind(
        problem.id,
        problem.created_at,
        problem.file_id,
        problem.visibility,
        problem.name,
        problem.label,
        problem.simfile,
        problem.owner_id,
        problem.updated_at
    );
    while (stmt.next()) {
        other_problems_copying_progress_printer.note_id(problem.id);
        main_sim.mysql.execute(
            InsertInto("problems (id, created_at, file_id, visibility, name, "
                       "label, simfile, owner_id, updated_at)")
                .values(
                    "?, ?, ?, ?, ?, ?, ?, ?, ?",
                    other_id_to_new_id(problem.id),
                    problem.created_at,
                    internal_files_merger.other_id_to_new_id(problem.file_id),
                    problem.visibility,
                    problem.name,
                    problem.label,
                    problem.simfile,
                    problem.owner_id
                        ? std::optional{users_merger.other_id_to_new_id(*problem.owner_id)}
                        : std::nullopt,
                    problem.updated_at
                )
        );
    }
}

} // namespace mergers
