#include <sim/contest_permissions.hh>

namespace sim::contest {

OverallPermissions get_overall_permissions(std::optional<User::Type> user_type) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = OverallPermissions;

    if (not user_type) {
        return PERM::VIEW_PUBLIC;
    }

    switch (*user_type) {
    case User::Type::ADMIN:
        return PERM::VIEW_ALL | PERM::VIEW_PUBLIC | PERM::ADD_PRIVATE | PERM::ADD_PUBLIC;
    case User::Type::TEACHER: return PERM::VIEW_PUBLIC | PERM::ADD_PRIVATE;
    case User::Type::NORMAL: return PERM::VIEW_PUBLIC;
    }

    __builtin_unreachable();
}

Permissions get_permissions(
    std::optional<User::Type> user_type, bool contest_is_public,
    std::optional<ContestUser::Mode> cu_mode) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = Permissions;
    using CUM = ContestUser::Mode;

    if (not user_type) {
        return (contest_is_public ? PERM::VIEW : PERM::NONE);
    }

    if (*user_type == User::Type::ADMIN) {
        return PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN | PERM::DELETE |
            PERM::MAKE_PUBLIC | PERM::SELECT_FINAL_SUBMISSIONS |
            PERM::VIEW_ALL_CONTEST_SUBMISSIONS | PERM::MANAGE_CONTEST_ENTRY_TOKEN;
    }

    if (not cu_mode.has_value()) {
        return (contest_is_public ? PERM::VIEW | PERM::PARTICIPATE : PERM::NONE);
    }

    switch (cu_mode.value()) {
    case CUM::OWNER:
        return PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN | PERM::DELETE |
            PERM::SELECT_FINAL_SUBMISSIONS | PERM::VIEW_ALL_CONTEST_SUBMISSIONS |
            PERM::MANAGE_CONTEST_ENTRY_TOKEN;

    case CUM::MODERATOR:
        return PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN | PERM::SELECT_FINAL_SUBMISSIONS |
            PERM::VIEW_ALL_CONTEST_SUBMISSIONS | PERM::MANAGE_CONTEST_ENTRY_TOKEN;

    case CUM::CONTESTANT: return PERM::VIEW | PERM::PARTICIPATE;
    }

    __builtin_unreachable();
}

} // namespace sim::contest
