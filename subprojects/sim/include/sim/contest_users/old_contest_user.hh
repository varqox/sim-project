#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/contests/old_contest.hh>
#include <sim/primary_key.hh>
#include <sim/users/old_user.hh>
#include <sim/users/user.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim::contest_users {

struct OldContestUser {
    ENUM_WITH_STRING_CONVERSIONS(Mode, uint8_t,
        (CONTESTANT, 0, "contestant")
        (MODERATOR, 1, "moderator")
        (OWNER, 2, "owner")
    );

    decltype(users::User::id) user_id;
    decltype(contests::OldContest::id) contest_id;
    EnumVal<Mode> mode;

    static constexpr auto primary_key =
        PrimaryKey{&OldContestUser::user_id, &OldContestUser::contest_id};
};

} // namespace sim::contest_users
