#pragma once

#include <optional>
#include <sim/contests/old_contest.hh>
#include <sim/contests/permissions.hh>
#include <sim/old_sql_fields/inf_datetime.hh>
#include <simlib/enum_val.hh>

namespace sim::contests {

enum class GetIdKind { CONTEST, CONTEST_ROUND, CONTEST_PROBLEM };

template <class T, class U>
std::optional<std::pair<OldContest, Permissions>>
get(sim::mysql::Connection& mysql,
    GetIdKind id_kind,
    T&& id,
    std::optional<U> user_id,
    CStringView curr_date) {
    STACK_UNWINDING_MARK;

    InplaceBuff<64> fields{"c.id, c.name, c.is_public"};
    bool check_round_begins = false;
    StringView id_part_sql;
    switch (id_kind) {
    case GetIdKind::CONTEST: {
        fields.append(", NULL");
        id_part_sql = "WHERE c.id=?";
        break;
    }
    case GetIdKind::CONTEST_ROUND: {
        check_round_begins = true;
        fields.append(", cr.begins");
        id_part_sql = "JOIN contest_rounds cr ON cr.id=? AND cr.contest_id=c.id";
        break;
    }
    case GetIdKind::CONTEST_PROBLEM: {
        check_round_begins = true;
        fields.append(", cr.begins");
        id_part_sql = "JOIN contest_problems cp ON cp.id=? AND cp.contest_id=c.id "
                      "JOIN contest_rounds cr ON cp.contest_round_id=cr.id";
        break;
    }
    }

    OldContest contest;
    old_mysql::Optional<EnumVal<decltype(users::User::type)>> user_type;
    old_mysql::Optional<decltype(contest_users::OldContestUser::mode)> cu_mode;
    old_mysql::Optional<old_sql_fields::InfDatetime> round_begins;

    old_mysql::Statement stmt;
    auto old_mysql = old_mysql::ConnectionView{mysql};
    if (user_id) {
        fields.append(", u.type, cu.mode");
        stmt = old_mysql.prepare(
            "SELECT ",
            fields,
            " "
            "FROM contests c "
            "LEFT JOIN users u ON u.id=? "
            "LEFT JOIN contest_users cu ON cu.contest_id=c.id"
            " AND cu.user_id=u.id ",
            id_part_sql
        );
        stmt.bind_and_execute(user_id.value(), id);
        stmt.res_bind_all(
            contest.id, contest.name, contest.is_public, round_begins, user_type, cu_mode
        );

    } else {
        stmt = old_mysql.prepare("SELECT ", fields, " FROM contests c ", id_part_sql);
        stmt.bind_and_execute(id);
        stmt.res_bind_all(contest.id, contest.name, contest.is_public, round_begins);
    }

    if (not stmt.next()) {
        return std::nullopt;
    }

    auto contest_perms = get_permissions(user_type, contest.is_public, cu_mode);
    if (check_round_begins and uint(~contest_perms & Permissions::ADMIN) and
        round_begins.value() > curr_date)
    {
        return std::nullopt;
    }

    return {{contest, contest_perms}};
}

} // namespace sim::contests
