#pragma once

#include "sim/contests/contest.hh"
#include "sim/users/user.hh"
#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"

#include <cstdint>

namespace sim::contest_users {

struct ContestUser {
    struct {
        decltype(users::User::id) user_id;
        decltype(contests::Contest::id) contest_id;
    } id;

    ENUM_WITH_STRING_CONVERSIONS(
        Mode, uint8_t,
        (CONTESTANT, 0, "contestant") //
        (MODERATOR, 1, "moderator") //
        (OWNER, 2, "owner") //
    );
    EnumVal<Mode> mode;
};

} // namespace sim::contest_users
