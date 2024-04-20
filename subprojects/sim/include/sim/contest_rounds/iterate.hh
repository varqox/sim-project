#pragma once

#include <sim/contest_rounds/old_contest_round.hh>
#include <sim/contests/permissions.hh>
#include <sim/old_mysql/old_mysql.hh>

namespace sim::contest_rounds {

enum class IterateIdKind { CONTEST, CONTEST_ROUND, CONTEST_PROBLEM };

template <class T, class Func>
void iterate(
    sim::mysql::Connection& mysql,
    IterateIdKind id_kind,
    T&& id,
    contests::Permissions contest_perms,
    CStringView curr_date,
    Func&& contest_round_processor
) {
    STACK_UNWINDING_MARK;
    static_assert(std::is_invocable_v<Func, const OldContestRound&>);

    StringView id_part_sql = [&]() -> StringView {
        switch (id_kind) {
        case IterateIdKind::CONTEST: return "WHERE cr.contest_id=?";
        case IterateIdKind::CONTEST_ROUND: return "WHERE cr.id=?";
        case IterateIdKind::CONTEST_PROBLEM:
            return "JOIN contest_problems cp ON cp.id=? AND "
                   "cp.contest_round_id=cr.id WHERE TRUE";
        }
        __builtin_unreachable();
    }();

    bool show_all = uint(contest_perms & contests::Permissions::ADMIN);
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare(
        "SELECT cr.id, cr.contest_id, cr.name, cr.item, cr.begins, cr.ends,"
        " cr.full_results, cr.ranking_exposure "
        "FROM contest_rounds cr ",
        id_part_sql,
        (show_all ? "" : " AND begins<=?")
    );

    if (show_all) {
        stmt.bind_and_execute(id);
    } else {
        stmt.bind_and_execute(id, curr_date);
    }

    OldContestRound cr;
    stmt.res_bind_all(
        cr.id,
        cr.contest_id,
        cr.name,
        cr.item,
        cr.begins,
        cr.ends,
        cr.full_results,
        cr.ranking_exposure
    );

    while (stmt.next()) {
        contest_round_processor(static_cast<const OldContestRound&>(cr));
    }
}

} // namespace sim::contest_rounds
