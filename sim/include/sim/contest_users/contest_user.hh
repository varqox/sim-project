#pragma once

#include <cstdint>
#include <sim/contests/contest.hh>
#include <sim/primary_key.hh>
#include <sim/users/user.hh>
#include <simlib/enum_val.hh>
#include <simlib/enum_with_string_conversions.hh>

namespace sim::contest_users {

struct ContestUser {
    ENUM_WITH_STRING_CONVERSIONS(Mode, uint8_t,
        (CONTESTANT, 0, "contestant")
        (MODERATOR, 1, "moderator")
        (OWNER, 2, "owner")
    );

    decltype(users::User::id) user_id;
    decltype(contests::Contest::id) contest_id;
    EnumVal<Mode> mode;

    static constexpr auto primary_key = PrimaryKey{&ContestUser::user_id, &ContestUser::contest_id};
};

} // namespace sim::contest_users
