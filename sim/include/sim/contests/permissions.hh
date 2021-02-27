#pragma once

#include "sim/contest_users/contest_user.hh"
#include "sim/users/user.hh"
#include "simlib/meta.hh"
#include "simlib/mysql/mysql.hh"

#include <optional>

namespace sim::contests {

enum class OverallPermissions : uint16_t {
    NONE = 0,
    VIEW_ALL = 1,
    VIEW_PUBLIC = 1 << 1,
    ADD_PRIVATE = 1 << 2,
    ADD_PUBLIC = 1 << 3,
};

DECLARE_ENUM_UNARY_OPERATOR(OverallPermissions, ~)
DECLARE_ENUM_OPERATOR(OverallPermissions, |)
DECLARE_ENUM_OPERATOR(OverallPermissions, &)

OverallPermissions
get_overall_permissions(std::optional<users::User::Type> user_type) noexcept;

enum class Permissions : uint16_t {
    NONE = 0,
    VIEW = 1,
    PARTICIPATE = 1 << 1,
    ADMIN = 1 << 2,
    DELETE = 1 << 3,
    MAKE_PUBLIC = 1 << 4,
    SELECT_FINAL_SUBMISSIONS = 1 << 5,
    VIEW_ALL_CONTEST_SUBMISSIONS = 1 << 6,
    MANAGE_CONTEST_ENTRY_TOKEN = 1 << 7
};

DECLARE_ENUM_UNARY_OPERATOR(Permissions, ~)
DECLARE_ENUM_OPERATOR(Permissions, |)
DECLARE_ENUM_OPERATOR(Permissions, &)

Permissions get_permissions(
    std::optional<users::User::Type> user_type, bool contest_is_public,
    std::optional<contest_users::ContestUser::Mode> cu_mode) noexcept;

template <class T, class U = uint64_t>
std::optional<Permissions>
get_permissions(::mysql::Connection& mysql, T&& contest_id, std::optional<U> user_id) {
    STACK_UNWINDING_MARK;

    uint8_t is_public = false;
    mysql::Optional<decltype(users::User::type)> user_type;
    mysql::Optional<decltype(contest_users::ContestUser::mode)> cu_mode;

    mysql::Statement stmt;
    if (user_id) {
        stmt = mysql.prepare("SELECT c.is_public, u.type, cu.mode FROM contests c "
                             "LEFT JOIN users u ON u.id=? "
                             "LEFT JOIN contest_users cu ON cu.contest_id=c.id"
                             " AND cu.user_id=u.id "
                             "WHERE c.id=?");
        stmt.bind_and_execute(user_id.value(), contest_id);
        stmt.res_bind_all(is_public, user_type, cu_mode);
    } else {
        stmt = mysql.prepare("SELECT is_public FROM contests WHERE id=?");
        stmt.bind_and_execute(contest_id);
        stmt.res_bind_all(is_public);
    }

    if (not stmt.next()) {
        return std::nullopt;
    }

    return get_permissions(user_type, is_public, cu_mode);
}

} // namespace sim::contests
