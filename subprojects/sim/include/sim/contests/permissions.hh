#pragma once

#include <optional>
#include <sim/contest_users/old_contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/mysql/mysql.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_operator_macros.hh>

namespace sim::contests {

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
    std::optional<users::User::Type> user_type,
    bool contest_is_public,
    std::optional<contest_users::OldContestUser::Mode> cu_mode
) noexcept;

template <class T, class U = uint64_t>
std::optional<Permissions>
get_permissions(sim::mysql::Connection& mysql, const T& contest_id, std::optional<U> user_id) {
    STACK_UNWINDING_MARK;

    decltype(Contest::is_public) is_public;
    std::optional<decltype(users::User::type)> user_type;
    std::optional<contest_users::OldContestUser::Mode> cu_mode;

    auto stmt = [&] {
        if (user_id) {
            auto res = mysql.execute(sql::Select("c.is_public, u.type, cu.mode")
                                         .from("contests c")
                                         .left_join("users u")
                                         .on("u.id=?", *user_id)
                                         .left_join("contest_users cu")
                                         .on("cu.contest_id=c.id AND cu.user_id=u.id")
                                         .where("c.id=?", contest_id));
            res.res_bind(is_public, user_type, cu_mode);
            return res;
        }

        auto res =
            mysql.execute(sql::Select("is_public").from("contests").where("id=?", contest_id));
        res.res_bind(is_public);
        return res;
    }();

    if (not stmt.next()) {
        return std::nullopt;
    }

    return get_permissions(user_type, is_public, cu_mode);
}

} // namespace sim::contests
