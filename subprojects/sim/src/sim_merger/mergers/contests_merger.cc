#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contests_merger.hh"
#include "modify_main_table_ids.hh"
#include "upadte_jobs_using_table_ids.hh"

#include <cstdint>
#include <optional>
#include <sim/contests/contest.hh>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contests::Contest;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
ContestsIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;
    using sim::jobs::Job;

    auto stmt = mysql.execute(Select("created_at")
                                  .from("jobs")
                                  .where("aux_id=? AND type=?", id, Job::Type::DELETE_CONTEST)
                                  .order_by("id")
                                  .limit("1"));
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return created_at;
}

ContestsMerger::ContestsMerger(const SimInstance& main_sim, const SimInstance& other_sim)
: main_sim{main_sim}
, other_sim{other_sim}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = ContestsIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void ContestsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "contests",
        [this](decltype(Contest::id) main_id) { return main_id_to_new_id(main_id); }
    );

    update_jobs_using_table_ids(
        "contests",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(Contest::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(Contest::id) main_id, decltype(Contest::id) new_id) {
            using sim::jobs::Job;
            STACK_UNWINDING_MARK;

            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id=?", new_id)
                    .where("type=? AND aux_id=?", Job::Type::DELETE_CONTEST, main_id)
            );
        }
    );

    // Save other contests
    auto other_contests_copying_progress_printer =
        ProgressPrinter<decltype(Contest::id)>{"progress: copying other contest with id:"};
    static_assert(Contest::COLUMNS_NUM == 4, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, created_at, name, is_public").from("contests").order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    Contest contest;
    stmt.res_bind(contest.id, contest.created_at, contest.name, contest.is_public);
    while (stmt.next()) {
        other_contests_copying_progress_printer.note_id(contest.id);
        main_sim.mysql.execute(InsertInto("contests (id, created_at, name, is_public)")
                                   .values(
                                       "?, ?, ?, ?",
                                       other_id_to_new_id(contest.id),
                                       contest.created_at,
                                       contest.name,
                                       contest.is_public
                                   ));
    }
}

} // namespace mergers
