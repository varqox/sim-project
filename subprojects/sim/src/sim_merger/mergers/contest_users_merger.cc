#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_users_merger.hh"
#include "contests_merger.hh"
#include "users_merger.hh"

#include <sim/contest_users/contest_user.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <string>

using sim::contest_users::ContestUser;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

ContestUsersMerger::ContestUsersMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const ContestsMerger& contests_merger,
    const UsersMerger& users_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, contests_merger{contests_merger}
, users_merger{users_merger} {}

void ContestUsersMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    // Save other contest_users
    auto other_contest_users_copying_progress_printer =
        ProgressPrinter<std::string>{"progress: copying other contest_user with id:"};
    static_assert(ContestUser::COLUMNS_NUM == 3, "Update the statements below");
    auto stmt = other_sim.mysql.execute(Select("user_id, contest_id, mode")
                                            .from("contest_users")
                                            .order_by("user_id DESC, contest_id DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    ContestUser contest_user;
    stmt.res_bind(contest_user.user_id, contest_user.contest_id, contest_user.mode);
    while (stmt.next()) {
        other_contest_users_copying_progress_printer.note_id(
            concat_tostr(contest_user.user_id, ',', contest_user.contest_id)
        );
        main_sim.mysql.execute(InsertInto("contest_users (user_id, contest_id, mode)")
                                   .values(
                                       "?, ?, ?",
                                       users_merger.other_id_to_new_id(contest_user.user_id),
                                       contests_merger.other_id_to_new_id(contest_user.contest_id),
                                       contest_user.mode
                                   ));
    }
}

} // namespace mergers
