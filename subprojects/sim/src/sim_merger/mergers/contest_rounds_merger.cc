#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_rounds_merger.hh"
#include "contests_merger.hh"
#include "modify_main_table_ids.hh"
#include "upadte_jobs_using_table_ids.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contest_rounds::ContestRound;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
ContestRoundsIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(Select("created_at")
                                  .from("jobs")
                                  .where("aux_id=? AND type=?", id, Job::Type::DELETE_CONTEST_ROUND)
                                  .order_by("id")
                                  .limit("1"));
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return created_at;
}

ContestRoundsMerger::ContestRoundsMerger(
    const SimInstance& main_sim, const SimInstance& other_sim, const ContestsMerger& contests_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, contests_merger{contests_merger}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = ContestRoundsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void ContestRoundsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "contest_rounds",
        [this](decltype(ContestRound::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "contest_rounds",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(ContestRound::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(ContestRound::id) main_id, decltype(ContestRound::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id=?", new_id)
                    .where("type=? AND aux_id=?", Job::Type::DELETE_CONTEST_ROUND, main_id)
            );
        }
    );

    // Save other contest_rounds
    auto other_contest_rounds_copying_progress_printer =
        ProgressPrinter<decltype(ContestRound::id)>{"progress: copying other contest_round with id:"
        };
    static_assert(ContestRound::COLUMNS_NUM == 9, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select(
            "id, created_at, contest_id, name, item, begins, ends, full_results, ranking_exposure"
        )
            .from("contest_rounds")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    ContestRound contest_round;
    stmt.res_bind(
        contest_round.id,
        contest_round.created_at,
        contest_round.contest_id,
        contest_round.name,
        contest_round.item,
        contest_round.begins,
        contest_round.ends,
        contest_round.full_results,
        contest_round.ranking_exposure
    );
    while (stmt.next()) {
        other_contest_rounds_copying_progress_printer.note_id(contest_round.id);
        main_sim.mysql.execute(InsertInto("contest_rounds (id, created_at, contest_id, name, item, "
                                          "begins, ends, full_results, ranking_exposure)")
                                   .values(
                                       "?, ?, ?, ?, ?, ?, ?, ?, ?",
                                       other_id_to_new_id(contest_round.id),
                                       contest_round.created_at,
                                       contests_merger.other_id_to_new_id(contest_round.contest_id),
                                       contest_round.name,
                                       contest_round.item,
                                       contest_round.begins,
                                       contest_round.ends,
                                       contest_round.full_results,
                                       contest_round.ranking_exposure
                                   ));
    }
}

} // namespace mergers
