#pragma once

#include "sim/contests/contest.hh"
#include "sim/users/user.hh"
#include "simlib/enum_val.hh"

namespace sim::contest_users {

struct ContestUser {
    struct {
        decltype(users::User::id) user_id;
        decltype(contests::Contest::id) contest_id;
    } id;

    enum class Mode : uint8_t { CONTESTANT = 0, MODERATOR = 1, OWNER = 2 };
    EnumVal<Mode> mode;
};

} // namespace sim::contest_users
