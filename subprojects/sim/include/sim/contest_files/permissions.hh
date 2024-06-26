#pragma once

#include <sim/contests/permissions.hh>
#include <simlib/enum_val.hh>

namespace sim::contest_files {

enum class OverallPermissions : uint16_t {
    NONE = 0,
    ADD = 1,
    VIEW_CREATOR = 1 << 1,
};

DECLARE_ENUM_UNARY_OPERATOR(OverallPermissions, ~)
DECLARE_ENUM_OPERATOR(OverallPermissions, |)
DECLARE_ENUM_OPERATOR(OverallPermissions, &)

OverallPermissions get_overall_permissions(contests::Permissions cperms) noexcept;

enum class Permissions : uint16_t {
    NONE = 0,
    VIEW = 1,
    DOWNLOAD = 1 << 1,
    EDIT = 1 << 2,
    DELETE = 1 << 3
};

DECLARE_ENUM_UNARY_OPERATOR(Permissions, ~)
DECLARE_ENUM_OPERATOR(Permissions, |)
DECLARE_ENUM_OPERATOR(Permissions, &)

Permissions get_permissions(contests::Permissions cperms) noexcept;

template <class T, class U = uint64_t>
std::optional<std::pair<Permissions, OverallPermissions>>
get_permissions(mysql::Connection& mysql, T&& contest_file_id, std::optional<U> user_id) {
    STACK_UNWINDING_MARK;

    uint8_t is_public = false;
    old_mysql::Optional<EnumVal<decltype(users::User::type)>> user_type;
    old_mysql::Optional<decltype(contest_users::OldContestUser::mode)> cu_mode;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare(
        "SELECT c.is_public",
        (user_id ? ", cu.mode, u.type " : " "),
        "FROM contest_files cf "
        "LEFT JOIN contests c ON c.id=cf.contest_id ",
        (user_id ? "LEFT JOIN contest_users cu ON cu.contest_id=cf.contest_id"
                   " AND cu.user_id=? "
                   "LEFT JOIN users u ON u.id=? "
                 : ""),
        "WHERE cf.id=?"
    );
    if (user_id) {
        stmt.bind_and_execute(user_id.value(), user_id.value(), contest_file_id);
        stmt.res_bind_all(is_public, cu_mode, user_type);
    } else {
        stmt.bind_and_execute(contest_file_id);
        stmt.res_bind_all(is_public);
    }

    if (not stmt.next()) {
        return std::nullopt;
    }

    auto cperms = contests::get_permissions(user_type.to_opt(), is_public, cu_mode.to_opt());
    return std::pair{get_permissions(cperms), get_overall_permissions(cperms)};
}

} // namespace sim::contest_files
