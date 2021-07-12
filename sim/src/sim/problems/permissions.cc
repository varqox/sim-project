#include "sim/problems/permissions.hh"
#include "simlib/debug.hh"

using sim::users::User;
using std::optional;

namespace sim::problems {

OverallPermissions get_overall_permissions(optional<User::Type> user_type) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = OverallPermissions;

    if (not user_type) {
        return PERM::VIEW_WITH_TYPE_PUBLIC;
    }

    switch (*user_type) {
    case User::Type::ADMIN:
        return PERM::VIEW_ALL | PERM::ADD | PERM::VIEW_WITH_TYPE_PUBLIC |
            PERM::VIEW_WITH_TYPE_CONTEST_ONLY | PERM::SELECT_BY_OWNER;
    case User::Type::TEACHER:
        return PERM::ADD | PERM::VIEW_WITH_TYPE_PUBLIC | PERM::VIEW_WITH_TYPE_CONTEST_ONLY |
            PERM::SELECT_BY_OWNER;
    case User::Type::NORMAL: return PERM::VIEW_WITH_TYPE_PUBLIC;
    }

    __builtin_unreachable();
}
Permissions get_permissions(
    optional<decltype(User::id)> user_id, optional<User::Type> user_type,
    decltype(Problem::owner) problem_owner, decltype(Problem::type) problem_type) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = Permissions;

    if (not user_type) {
        switch (problem_type) {
        case Problem::Type::PUBLIC: return PERM::VIEW | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS;
        case Problem::Type::CONTEST_ONLY:
        case Problem::Type::PRIVATE: return PERM::NONE;
        }
    }

    constexpr PERM admin_perms = PERM::VIEW | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
        PERM::VIEW_HIDDEN_TAGS | PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS | PERM::VIEW_SIMFILE |
        PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME | PERM::VIEW_RELATED_JOBS | PERM::DOWNLOAD |
        PERM::SUBMIT | PERM::EDIT | PERM::SUBMIT_IGNORED | PERM::REUPLOAD | PERM::REJUDGE_ALL |
        PERM::RESET_TIME_LIMITS | PERM::EDIT_TAGS | PERM::EDIT_HIDDEN_TAGS | PERM::DELETE |
        PERM::MERGE | PERM::CHANGE_STATEMENT | PERM::VIEW_ATTACHING_CONTEST_PROBLEMS;

    if (user_id and problem_owner and *user_id == *problem_owner) {
        return admin_perms;
    }

    switch (user_type.value()) {
    case User::Type::ADMIN: return admin_perms;
    case User::Type::TEACHER: {
        switch (problem_type) {
        case Problem::Type::PUBLIC:
            return PERM::VIEW | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
                PERM::VIEW_HIDDEN_TAGS | PERM::VIEW_SOLUTIONS_AND_SUBMISSIONS |
                PERM::VIEW_SIMFILE | PERM::VIEW_OWNER | PERM::VIEW_ADD_TIME | PERM::DOWNLOAD |
                PERM::SUBMIT | PERM::SUBMIT_IGNORED;
        case Problem::Type::CONTEST_ONLY:
            return PERM::VIEW | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS |
                PERM::VIEW_HIDDEN_TAGS | PERM::VIEW_SIMFILE | PERM::VIEW_OWNER |
                PERM::VIEW_ADD_TIME | PERM::SUBMIT | PERM::SUBMIT_IGNORED;
        case Problem::Type::PRIVATE: return PERM::NONE;
        }
        __builtin_unreachable();
    }
    case User::Type::NORMAL: {
        switch (problem_type) {
        case Problem::Type::PUBLIC:
            return PERM::VIEW | PERM::VIEW_STATEMENT | PERM::VIEW_TAGS | PERM::SUBMIT |
                PERM::SUBMIT_IGNORED;
        case Problem::Type::CONTEST_ONLY:
        case Problem::Type::PRIVATE: return PERM::NONE;
        }
        __builtin_unreachable();
    }
    }

    __builtin_unreachable();
}

} // namespace sim::problems
