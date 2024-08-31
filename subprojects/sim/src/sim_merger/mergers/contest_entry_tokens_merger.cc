#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_entry_tokens_merger.hh"
#include "contests_merger.hh"

#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/mysql/mysql.hh>
#include <sim/random.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contest_entry_tokens::ContestEntryToken;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;

namespace mergers {

ContestEntryTokensMerger::ContestEntryTokensMerger(
    const SimInstance& main_sim, const SimInstance& other_sim, const ContestsMerger& contests_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, contests_merger{contests_merger} {}

void ContestEntryTokensMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    // Save other contest_entry_tokens
    auto other_contest_files_copying_progress_printer =
        ProgressPrinter<decltype(ContestEntryToken::token)>{
            "progress: copying other contest_entry_token with token:"
        };
    static_assert(ContestEntryToken::COLUMNS_NUM == 4, "Update the statements below");
    auto stmt =
        other_sim.mysql.execute(Select("token, contest_id, short_token, short_token_expiration")
                                    .from("contest_entry_tokens")
                                    .order_by("token DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    ContestEntryToken contest_entry_token;
    stmt.res_bind(
        contest_entry_token.token,
        contest_entry_token.contest_id,
        contest_entry_token.short_token,
        contest_entry_token.short_token_expiration
    );
    while (stmt.next()) {
        other_contest_files_copying_progress_printer.note_id(contest_entry_token.token);
        for (;;) {
            auto insert_stmt = main_sim.mysql.execute(
                InsertIgnoreInto(
                    "contest_entry_tokens (token, contest_id, short_token, short_token_expiration)"
                )
                    .values(
                        "?, ?, ?, ?",
                        contest_entry_token.token,
                        contests_merger.other_id_to_new_id(contest_entry_token.contest_id),
                        contest_entry_token.short_token,
                        contest_entry_token.short_token_expiration
                    )
            );
            if (insert_stmt.affected_rows() == 1) {
                break;
            }
            // Change colliding tokens on collision
            int dummy;
            auto token_stmt = main_sim.mysql.execute(
                Select("1").from("contest_entry_tokens").where("token=?", contest_entry_token.token)
            );
            token_stmt.res_bind(dummy);
            if (token_stmt.next()) {
                contest_entry_token.token =
                    sim::generate_random_token(decltype(ContestEntryToken::token)::len);
            }
            if (contest_entry_token.short_token) {
                auto short_token_stmt = main_sim.mysql.execute(
                    Select("1")
                        .from("contest_entry_tokens")
                        .where("short_token=?", *contest_entry_token.short_token)
                );
                short_token_stmt.res_bind(dummy);
                if (short_token_stmt.next()) {
                    contest_entry_token.short_token =
                        sim::generate_random_token(decltype(ContestEntryToken::short_token
                        )::value_type::len);
                }
            }
        }
    }
}

} // namespace mergers
