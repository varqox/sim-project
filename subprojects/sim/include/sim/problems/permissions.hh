#pragma once

#include <sim/old_mysql/old_mysql.hh>
#include <sim/problems/old_problem.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_operator_macros.hh>

namespace sim::problems {

enum class OverallPermissions : uint32_t {
    NONE = 0,
    ADD = 1,
    VIEW_ALL = 1 << 1,
    VIEW_WITH_TYPE_PUBLIC = 1 << 2,
    VIEW_WITH_TYPE_CONTEST_ONLY = 1 << 5,
    SELECT_BY_OWNER = 1 << 4,
};

DECLARE_ENUM_UNARY_OPERATOR(OverallPermissions, ~)
DECLARE_ENUM_OPERATOR(OverallPermissions, |)
DECLARE_ENUM_OPERATOR(OverallPermissions, &)

OverallPermissions get_overall_permissions(std::optional<users::User::Type> user_type) noexcept;

enum class Permissions : uint32_t {
    NONE = 0,
    VIEW = 1,
    VIEW_STATEMENT = 1 << 1,
    VIEW_TAGS = 1 << 2,
    VIEW_HIDDEN_TAGS = 1 << 3,
    VIEW_SOLUTIONS_AND_SUBMISSIONS = 1 << 4,
    VIEW_SIMFILE = 1 << 5,
    VIEW_OWNER = 1 << 6,
    VIEW_ADD_TIME = 1 << 7,
    VIEW_RELATED_JOBS = 1 << 8,
    DOWNLOAD = 1 << 9,
    SUBMIT = 1 << 10,
    EDIT = 1 << 11,
    SUBMIT_IGNORED = 1 << 12,
    REUPLOAD = 1 << 13,
    REJUDGE_ALL = 1 << 14,
    RESET_TIME_LIMITS = 1 << 15,
    EDIT_TAGS = 1 << 16,
    EDIT_HIDDEN_TAGS = 1 << 17,
    DELETE = 1 << 18,
    MERGE = 1 << 19,
    CHANGE_STATEMENT = 1 << 20,
    VIEW_ATTACHING_CONTEST_PROBLEMS = 1 << 21,
};

DECLARE_ENUM_UNARY_OPERATOR(Permissions, ~)
DECLARE_ENUM_OPERATOR(Permissions, |)
DECLARE_ENUM_OPERATOR(Permissions, &)

Permissions get_permissions(
    std::optional<decltype(users::User::id)> user_id,
    std::optional<users::User::Type> user_type,
    decltype(OldProblem::owner_id) problem_owner_id,
    decltype(OldProblem::visibility) problem_visibility
) noexcept;

template <class T>
std::optional<Permissions> get_permissions(
    mysql::Connection& mysql,
    T&& problem_id,
    std::optional<decltype(users::User::id)> user_id,
    std::optional<users::User::Type> user_type
) {

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT owner_id, visibility FROM problems WHERE id=?");
    stmt.bind_and_execute(problem_id);

    old_mysql::Optional<decltype(OldProblem::owner_id)::value_type> problem_owner_id;
    decltype(OldProblem::visibility) problem_visibility;
    stmt.res_bind_all(problem_owner_id, problem_visibility);

    if (not stmt.next()) {
        return std::nullopt;
    }

    return get_permissions(user_id, user_type, problem_owner_id, problem_visibility);
}

} // namespace sim::problems
