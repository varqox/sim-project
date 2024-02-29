#include <sim/contest_users/contest_user.hh>
#include <sim/contests/permissions.hh>

using sim::contest_users::ContestUser;
using sim::users::User;

namespace sim::contests {

Permissions get_permissions(
    std::optional<User::Type> user_type,
    bool contest_is_public,
    std::optional<ContestUser::Mode> cu_mode
) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = Permissions;
    using CUM = ContestUser::Mode;

    if (not user_type) {
        return (contest_is_public ? PERM::VIEW : PERM::NONE);
    }

    if (*user_type == User::Type::ADMIN) {
        return PERM::VIEW | PERM::PARTICIPATE | PERM::ADMIN | PERM::DELETE | PERM::MAKE_PUBLIC |
            PERM::SELECT_FINAL_SUBMISSIONS | PERM::VIEW_ALL_CONTEST_SUBMISSIONS |
            PERM::MANAGE_CONTEST_ENTRY_TOKEN;
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

} // namespace sim::contests
